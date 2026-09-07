#include "features/adb_mode/adb_client.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

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

// 建立 ADB 连接：解析 endpoint → 检查已连接状态 → 执行 adb connect → 校验设备是否为 ready
auto connect(const AdbConnectionConfig& config) -> std::expected<AdbConnectionResult, std::string> {
  // 解析出目标 TCP endpoint (如 127.0.0.1:16384)
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
    // 先检查目标设备是否已经处于 ready 状态，避免重复连接
    auto devices_before_result = list_devices(config);
    if (!devices_before_result) {
      return std::unexpected(devices_before_result.error());
    }
    if (!has_ready_device(devices_before_result.value(), endpoint)) {
      // 未连接时发起 adb connect，并标记所有权归本次会话
      connected_by_us = true;
      auto connect_result = run(config, {L"connect", utils::string::FromUtf8(endpoint)});
      if (!connect_result) {
        return std::unexpected(connect_result.error());
      }
      if (connect_result->exit_code != 0) {
        return std::unexpected(make_command_error("ADB connect", connect_result.value()));
      }
    } else {
      // 已经存在外部活跃连接，不归本次会话管理生命周期
      return AdbConnectionResult{.serial = endpoint, .connected_by_us = false};
    }
  }

  // 再次读取设备列表确认连接后的最终状态
  auto devices_result = list_devices(config);
  if (!devices_result) {
    return std::unexpected(devices_result.error());
  }

  // 指定了明确序列号时，验证其是否处于正常可用 (device) 状态
  if (!config.serial.empty()) {
    if (!has_ready_device(devices_result.value(), config.serial)) {
      return std::unexpected(std::format("Configured ADB device is not ready: {}", config.serial));
    }
    return AdbConnectionResult{.serial = config.serial, .connected_by_us = connected_by_us};
  }

  // 自动选择模式下，确认刚刚连接的 endpoint 是否已就绪
  if (has_ready_device(devices_result.value(), endpoint)) {
    return AdbConnectionResult{.serial = endpoint, .connected_by_us = connected_by_us};
  }

  return std::unexpected(std::format(
      "ADB connected to {}, but the endpoint is not ready. Set the device serial explicitly if "
      "the emulator uses another serial.",
      endpoint));
}

// 断开 ADB 连接：校验 TCP serial 格式 → 执行 adb disconnect → 校验命令退出码
auto disconnect(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<void, std::string> {
  // 只断开网络 TCP 连接 (包含冒号端口)，不拔掉 USB 物理设备
  if (serial.empty() || serial.find(':') == std::string_view::npos) {
    return {};
  }

  // 执行 adb disconnect <endpoint>
  auto result = run(config, {L"disconnect", utils::string::FromUtf8(std::string(serial))});
  if (!result) {
    return std::unexpected(result.error());
  }
  if (result->exit_code != 0) {
    return std::unexpected(make_command_error("ADB disconnect", result.value()));
  }
  return {};
}

}  // namespace features::adb_mode::adb
