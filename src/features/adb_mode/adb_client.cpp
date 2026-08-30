#include "features/adb_mode/adb_client.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace features::adb_mode::adb {

namespace {

// 从命令结果中提取可读的错误文本，并补上操作名称和退出码。
auto make_command_error(std::string_view operation, const utils::process::CommandResult& result)
    -> std::string {
  // ADB 通常把诊断写到 stderr；若没有内容，再尝试使用 stdout。
  auto details = utils::string::TrimAscii(result.stderr_data);
  if (details.empty()) {
    details = utils::string::TrimAscii(result.stdout_data);
  }

  // 没有任何诊断文本时，至少保留退出码帮助定位失败原因。
  if (details.empty()) {
    return std::format("{} failed with exit code {}", operation, result.exit_code);
  }

  return std::format("{} failed with exit code {}: {}", operation, result.exit_code, details);
}

// 校验主机和端口，并拼出 adb connect 使用的 endpoint。
auto make_endpoint(const AdbConnectionConfig& config) -> std::expected<std::string, std::string> {
  if (config.host.empty()) {
    return std::unexpected("ADB host cannot be empty");
  }
  if (config.port <= 0 || config.port > 65535) {
    return std::unexpected("ADB port must be between 1 and 65535");
  }

  return std::format("{}:{}", config.host, config.port);
}

// 在命令参数前插入指定设备的 -s 选项。
auto build_device_arguments(std::string_view serial, const std::vector<std::wstring>& arguments)
    -> std::vector<std::wstring> {
  if (serial.empty()) {
    return arguments;
  }

  std::vector<std::wstring> result;
  result.reserve(arguments.size() + 2);
  // -s 必须放在 shell、exec-out 等具体子命令之前。
  result.emplace_back(L"-s");
  result.emplace_back(utils::string::FromUtf8(std::string(serial)));
  result.insert(result.end(), arguments.begin(), arguments.end());
  return result;
}

// 解析 adb devices -l 的逐行输出，只保留包含序列号和状态的记录。
auto parse_devices(std::string_view output) -> std::vector<AdbDevice> {
  std::vector<AdbDevice> devices;
  std::istringstream stream{std::string(output)};
  std::string line;

  while (std::getline(stream, line)) {
    // 跳过空行和 adb 固定输出的表头。
    auto trimmed = utils::string::TrimAscii(line);
    if (trimmed.empty() || trimmed.starts_with("List of devices attached")) {
      continue;
    }

    std::istringstream line_stream(trimmed);
    AdbDevice device;
    line_stream >> device.serial >> device.state;
    if (!device.serial.empty() && !device.state.empty()) {
      devices.push_back(std::move(device));
    }
  }

  return devices;
}

auto has_ready_device(const std::vector<AdbDevice>& devices, std::string_view serial) -> bool {
  return std::ranges::any_of(devices, [serial](const AdbDevice& device) {
    return device.serial == serial && device.state == "device";
  });
}

// 检查 screencap 输出是否包含完整的 PNG 头和有效尺寸。
auto is_valid_png(std::string_view png) -> bool {
  if (png.size() < 24) {
    return false;
  }

  const auto read_big_endian_u32 = [&png](std::size_t offset) -> std::uint32_t {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(png[offset])) << 24u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(png[offset + 1])) << 16u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(png[offset + 2])) << 8u) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(png[offset + 3]));
  };

  constexpr std::array<char, 8> kPngSignature = {static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n',
                                                 static_cast<char>(0x1A), '\n'};
  if (!std::equal(kPngSignature.begin(), kPngSignature.end(), png.begin()) ||
      png.substr(12, 4) != "IHDR") {
    return false;
  }

  const auto width = read_big_endian_u32(16);
  const auto height = read_big_endian_u32(20);
  if (width == 0 || height == 0 ||
      width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  return true;
}

}  // namespace

// 解析用户指定的 ADB 路径或 PATH 中的可执行文件名。
auto resolve_executable(std::string_view configured_path)
    -> std::expected<std::filesystem::path, std::string> {
  const auto trimmed_path = utils::string::TrimAscii(configured_path);
  if (trimmed_path.empty()) {
    return std::unexpected("Configured ADB executable path is empty");
  }

  const auto configured = std::filesystem::path(utils::string::FromUtf8(trimmed_path));
  std::error_code file_error;
  if (std::filesystem::is_regular_file(configured, file_error)) {
    return configured;
  }

  // 只填写 adb.exe/adb 时让 Windows 按 PATH 规则解析它。
  if (configured.parent_path().empty()) {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
      const auto length = SearchPathW(nullptr, configured.c_str(), nullptr,
                                      static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
      if (length == 0) {
        return std::unexpected("Configured ADB executable was not found: " + trimmed_path);
      }
      if (length < buffer.size()) {
        return std::filesystem::path(std::wstring(buffer.data(), length));
      }
      buffer.resize(static_cast<std::size_t>(length) + 1);
    }
  }

  return std::unexpected("Configured ADB executable does not exist: " + trimmed_path);
}

// 执行一条不绑定设备的 ADB 命令，并统一处理启动失败和超时。
auto run(const AdbConnectionConfig& config, const std::vector<std::wstring>& arguments,
         std::chrono::milliseconds timeout)
    -> std::expected<utils::process::CommandResult, std::string> {
  if (config.executable.empty()) {
    return std::unexpected("ADB executable path is empty");
  }

  auto result = utils::process::run(config.executable, arguments,
                                    utils::process::RunOptions{.timeout = timeout});
  if (!result) {
    return std::unexpected("Failed to execute ADB: " + result.error());
  }
  // 超时没有可靠的退出码，单独转换成执行器错误。
  if (result->timed_out) {
    return std::unexpected("ADB command timed out");
  }

  return result;
}

// 为指定设备执行 ADB 命令，并要求命令以零退出码结束。
auto run_on_device(const AdbConnectionConfig& config, std::string_view serial,
                   const std::vector<std::wstring>& arguments, std::chrono::milliseconds timeout)
    -> std::expected<utils::process::CommandResult, std::string> {
  if (serial.empty()) {
    return std::unexpected("ADB device serial cannot be empty");
  }

  auto result = run(config, build_device_arguments(serial, arguments), timeout);
  if (!result) {
    return std::unexpected(result.error());
  }
  // 设备命令的非零退出码代表业务失败，而不是进程启动失败。
  if (result->exit_code != 0) {
    return std::unexpected(make_command_error("ADB device command", result.value()));
  }

  return result;
}

// 获取当前 ADB 设备列表，供连接流程确认目标设备是否 ready。
auto list_devices(const AdbConnectionConfig& config)
    -> std::expected<std::vector<AdbDevice>, std::string> {
  auto result = run(config, {L"devices", L"-l"});
  if (!result) {
    return std::unexpected(result.error());
  }
  if (result->exit_code != 0) {
    return std::unexpected(make_command_error("ADB device listing", result.value()));
  }

  // 只解析 stdout；stderr 已在非零退出时转成错误。
  return parse_devices(result->stdout_data);
}

// 连接目标 endpoint，并从设备列表中确认最终可用的序列号。
auto connect(const AdbConnectionConfig& config) -> std::expected<AdbConnectionResult, std::string> {
  std::string endpoint;
  if (!config.serial.empty() && config.serial.find(':') != std::string::npos) {
    endpoint = config.serial;
  } else if (config.serial.empty()) {
    auto endpoint_result = make_endpoint(config);
    if (!endpoint_result) {
      return std::unexpected(endpoint_result.error());
    }
    endpoint = endpoint_result.value();
  }

  bool connected_by_us = false;
  if (!endpoint.empty()) {
    // 已经 ready 的 TCP 设备不需要重复 connect，也不归本次会话所有。
    auto devices_before_result = list_devices(config);
    if (!devices_before_result) {
      return std::unexpected(devices_before_result.error());
    }
    if (!has_ready_device(devices_before_result.value(), endpoint)) {
      // 只有设备尚未 ready 时才建立本次会话负责的 TCP 连接。
      connected_by_us = true;
      auto connect_result = run(config, {L"connect", utils::string::FromUtf8(endpoint)});
      if (!connect_result) {
        return std::unexpected(connect_result.error());
      }
      if (connect_result->exit_code != 0) {
        return std::unexpected(make_command_error("ADB connect", connect_result.value()));
      }
    } else {
      return AdbConnectionResult{.serial = endpoint, .connected_by_us = false};
    }
  }

  auto devices_result = list_devices(config);
  if (!devices_result) {
    return std::unexpected(devices_result.error());
  }

  if (!config.serial.empty()) {
    // 显式序列号必须在列表中处于 device 状态，不能只连接到 offline/unauthorized 设备。
    if (!has_ready_device(devices_result.value(), config.serial)) {
      return std::unexpected(std::format("Configured ADB device is not ready: {}", config.serial));
    }
    return AdbConnectionResult{.serial = config.serial, .connected_by_us = connected_by_us};
  }

  // 自动选择时只接受刚刚连接的 endpoint，防止误选其他模拟器实例。
  if (has_ready_device(devices_result.value(), endpoint)) {
    return AdbConnectionResult{.serial = endpoint, .connected_by_us = connected_by_us};
  }

  return std::unexpected(std::format(
      "ADB connected to {}, but the endpoint is not ready. Set the device serial explicitly if "
      "the emulator uses another serial.",
      endpoint));
}

// 断开本模块建立的 TCP ADB 连接。
auto disconnect(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<void, std::string> {
  if (serial.empty() || serial.find(':') == std::string_view::npos) {
    return {};
  }

  auto result = run(config, {L"disconnect", utils::string::FromUtf8(std::string(serial))});
  if (!result) {
    return std::unexpected(result.error());
  }
  if (result->exit_code != 0) {
    return std::unexpected(make_command_error("ADB disconnect", result.value()));
  }
  return {};
}

// 抓取设备屏幕、校验 PNG，并通过临时文件替换目标文件。
auto capture_screen_to_file(const AdbConnectionConfig& config, std::string_view serial,
                            const std::filesystem::path& output_path)
    -> std::expected<void, std::string> {
  if (output_path.empty()) {
    return std::unexpected("Screenshot output path cannot be empty");
  }

  // 使用 exec-out 保持 PNG 二进制原样输出，并给截图命令更长的超时时间。
  auto result =
      run_on_device(config, serial, {L"exec-out", L"screencap", L"-p"}, std::chrono::seconds(30));
  if (!result) {
    return std::unexpected(result.error());
  }

  if (!is_valid_png(result->stdout_data)) {
    return std::unexpected("ADB screencap returned an invalid PNG image");
  }

  const auto parent = output_path.parent_path();
  if (!parent.empty()) {
    auto ensure_result = utils::path::EnsureDirectoryExists(parent);
    if (!ensure_result) {
      return std::unexpected("Failed to create screenshot directory: " + ensure_result.error());
    }
  }

  auto temporary_path = output_path;
  temporary_path += L".tmp";

  std::error_code remove_error;
  std::filesystem::remove(temporary_path, remove_error);

  {
    // 先完整写入 .tmp；写失败时删除临时文件，不破坏旧截图。
    std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
    if (!file) {
      return std::unexpected("Failed to open temporary screenshot file: " +
                             temporary_path.string());
    }
    file.write(result->stdout_data.data(),
               static_cast<std::streamsize>(result->stdout_data.size()));
    if (!file) {
      file.close();
      std::filesystem::remove(temporary_path, remove_error);
      return std::unexpected("Failed to write screenshot file: " + temporary_path.string());
    }
  }

  std::error_code rename_error;
  // 写入成功后再一次性改名为目标文件，减少半张截图暴露给读取方的机会。
  std::filesystem::rename(temporary_path, output_path, rename_error);
  if (rename_error) {
    std::filesystem::remove(temporary_path, remove_error);
    return std::unexpected("Failed to finalize screenshot file: " + rename_error.message());
  }

  return {};
}

}  // namespace features::adb_mode::adb
