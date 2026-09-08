#include "features/adb_mode/device_finder.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/tlhelp32.hpp"

#include "features/adb_mode/adb_client.hpp"
#include "utils/path/path.hpp"
#include "utils/process/process.hpp"
#include "utils/string/string.hpp"

namespace features::adb_mode::device_finder {

namespace {

enum class EmulatorKind : std::uint8_t {
  MuMu,
  LDPlayer,
  BlueStacks,
};

struct EmulatorCandidate {
  EmulatorKind kind;
  std::filesystem::path adb_path;
  std::filesystem::path management_tool;
};

struct DiscoveredEndpoint {
  EmulatorKind kind;
  std::filesystem::path adb_path;
  std::string serial;
  std::string host;
  int port = 0;
};

auto path_to_utf8(const std::filesystem::path& path) -> std::string {
  return utils::string::ToUtf8(path.wstring());
}

auto is_existing_regular_file(const std::filesystem::path& path) -> bool {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error);
}

// 确认文件存在，并尽量转换为稳定的规范路径供去重使用。
auto existing_file(const std::filesystem::path& path) -> std::optional<std::filesystem::path> {
  if (!is_existing_regular_file(path)) {
    return std::nullopt;
  }

  std::error_code error;
  auto normalized = std::filesystem::weakly_canonical(path, error);
  return error ? std::optional<std::filesystem::path>{path}
               : std::optional<std::filesystem::path>{normalized};
}

auto same_path(const std::filesystem::path& left, const std::filesystem::path& right) -> bool {
  return utils::path::NormalizeForComparison(left) == utils::path::NormalizeForComparison(right);
}

// 读取进程的完整镜像路径，用来从正在运行的模拟器反推出安装目录。
auto get_process_image_path(DWORD process_id) -> std::optional<std::filesystem::path> {
  // 只申请查询权限，避免为了识别进程请求不必要的访问权限。
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (!process) {
    return std::nullopt;
  }

  std::vector<wchar_t> buffer(32 * 1024);
  DWORD length = static_cast<DWORD>(buffer.size());
  const BOOL succeeded = QueryFullProcessImageNameW(process, 0, buffer.data(), &length);
  CloseHandle(process);

  if (!succeeded || length == 0) {
    return std::nullopt;
  }

  return std::filesystem::path(std::wstring(buffer.data(), length));
}

auto to_lower(std::wstring value) -> std::wstring {
  std::ranges::transform(value, value.begin(), [](wchar_t character) {
    return static_cast<wchar_t>(std::towlower(character));
  });
  return value;
}

// 根据进程名判断它属于 MuMu、LDPlayer 还是 BlueStacks。
auto process_kind(std::wstring_view process_name) -> std::optional<EmulatorKind> {
  const auto name = to_lower(std::wstring(process_name));
  if (name.find(L"mumunxdevice.exe") != std::wstring::npos) {
    return EmulatorKind::MuMu;
  }
  if (name.find(L"dnplayer.exe") != std::wstring::npos) {
    return EmulatorKind::LDPlayer;
  }
  if (name.find(L"hd-player.exe") != std::wstring::npos) {
    return EmulatorKind::BlueStacks;
  }
  return std::nullopt;
}

// 验证 ADB 和管理工具路径，并将同一 adb.exe 去重后加入候选列表。
auto add_candidate(std::vector<EmulatorCandidate>& candidates, EmulatorKind kind,
                   const std::filesystem::path& adb_path,
                   const std::filesystem::path& management_tool) -> void {
  auto existing_adb = existing_file(adb_path);
  if (!existing_adb) {
    return;
  }

  if (std::ranges::any_of(candidates, [&existing_adb](const EmulatorCandidate& candidate) {
        return same_path(candidate.adb_path, *existing_adb);
      })) {
    // 同一 ADB 可能从多个进程路径推导出来，只保留一份候选。
    return;
  }

  auto existing_tool = existing_file(management_tool);
  candidates.push_back(EmulatorCandidate{
      .kind = kind,
      .adb_path = *existing_adb,
      .management_tool = existing_tool.value_or(std::filesystem::path{}),
  });
}

// 扫描当前进程，定位正在运行的 MuMu/LDPlayer/BlueStacks 及其配套 ADB。
auto find_running_emulators() -> std::vector<EmulatorCandidate> {
  std::vector<EmulatorCandidate> candidates;

  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return candidates;
  }

  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (!Process32FirstW(snapshot, &entry)) {
    CloseHandle(snapshot);
    return candidates;
  }

  do {
    const auto kind = process_kind(entry.szExeFile);
    if (!kind) {
      continue;
    }

    const auto process_path = get_process_image_path(entry.th32ProcessID);
    if (!process_path) {
      continue;
    }

    const auto process_directory = process_path->parent_path();
    if (*kind == EmulatorKind::MuMu) {
      // MuMu v5+ 的 MuMuNxDevice.exe 位于 nx_device/.../shell 下。
      const auto adb_path = process_directory / L".." / L".." / L".." / L"nx_main" / L"adb.exe";
      add_candidate(candidates, *kind, adb_path, adb_path.parent_path() / L"MuMuManager.exe");
      add_candidate(candidates, *kind, process_directory / L"adb.exe",
                    process_directory / L"MuMuManager.exe");
      continue;
    }

    // LDPlayer 的 adb.exe 与 ldconsole.exe 通常位于同一目录。
    if (*kind == EmulatorKind::LDPlayer) {
      add_candidate(candidates, *kind, process_directory / L"adb.exe",
                    process_directory / L"ldconsole.exe");
      continue;
    }

    // BlueStacks 的 ADB 按已知目录布局查找，并按顺序使用首个存在的文件。
    const std::array<std::filesystem::path, 2> adb_relative_paths = {
        std::filesystem::path(L"HD-Adb.exe"),
        std::filesystem::path(L"Engine") / L"ProgramFiles" / L"HD-Adb.exe",
    };
    for (const auto& relative_path : adb_relative_paths) {
      const auto adb_path = process_directory / relative_path;
      if (!existing_file(adb_path)) {
        continue;
      }
      // 同一进程只使用首个匹配路径，避免一个实例产生两个 ADB 候选。
      add_candidate(candidates, *kind, adb_path, {});
      break;
    }
  } while (Process32NextW(snapshot, &entry));

  CloseHandle(snapshot);
  return candidates;
}

auto make_command_error(std::string_view operation, const utils::process::CommandResult& result)
    -> std::string {
  auto details = utils::string::TrimAscii(result.stderr_data);
  if (details.empty()) {
    details = utils::string::TrimAscii(result.stdout_data);
  }

  if (details.empty()) {
    return std::format("{} failed with exit code {}", operation, result.exit_code);
  }
  return std::format("{} failed with exit code {}: {}", operation, result.exit_code, details);
}

// 执行 MuMuManager/LDConsole，并统一处理工作目录、超时和退出码。
auto run_management_tool(const std::filesystem::path& executable,
                         const std::vector<std::wstring>& arguments, std::string_view operation)
    -> std::expected<utils::process::CommandResult, std::string> {
  if (executable.empty() || !is_existing_regular_file(executable)) {
    return std::unexpected(std::format("{} executable was not found", operation));
  }

  auto result = utils::process::run(executable, arguments,
                                    utils::process::RunOptions{
                                        .timeout = std::chrono::seconds(10),
                                        .working_directory = executable.parent_path(),
                                    });
  if (!result) {
    return std::unexpected(std::format("{} could not be started: {}", operation, result.error()));
  }
  if (result->timed_out) {
    return std::unexpected(std::format("{} timed out", operation));
  }
  // 管理工具的非零退出码表示发现操作失败，不能继续解析半成品输出。
  if (result->exit_code != 0) {
    return std::unexpected(make_command_error(operation, result.value()));
  }
  return result;
}

// 将一段只包含十进制整数的文本安全转换为 int。
auto parse_integer(std::string_view value) -> std::optional<int> {
  const auto trimmed = utils::string::TrimAscii(value);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  int parsed = 0;
  const auto [end, error] =
      std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
  if (error != std::errc{} || end != trimmed.data() + trimmed.size()) {
    return std::nullopt;
  }
  return parsed;
}

// 按逗号拆分 LDConsole 的一行，并去除每个字段两侧空白。
auto split_csv(std::string_view line) -> std::vector<std::string> {
  std::vector<std::string> fields;
  std::size_t begin = 0;
  while (begin <= line.size()) {
    const auto separator = line.find(',', begin);
    const auto end = separator == std::string_view::npos ? line.size() : separator;
    fields.emplace_back(utils::string::TrimAscii(line.substr(begin, end - begin)));
    if (separator == std::string_view::npos) {
      break;
    }
    begin = separator + 1;
  }
  return fields;
}

// 加入一个发现到的 endpoint，并按 ADB 路径和序列号去重。
auto add_endpoint(std::vector<DiscoveredEndpoint>& endpoints, EmulatorKind kind,
                  const std::filesystem::path& adb_path, std::string host, int port,
                  std::string serial = {}) -> void {
  if (serial.empty()) {
    // MuMu 输出 host/port，先校验并按 ADB 约定合成 serial。
    if (host.empty() || port <= 0 || port > 65535) {
      return;
    }
    serial = std::format("{}:{}", host, port);
  }

  if (std::ranges::any_of(endpoints, [&adb_path, &serial](const DiscoveredEndpoint& endpoint) {
        return endpoint.serial == serial && same_path(endpoint.adb_path, adb_path);
      })) {
    // 管理工具可能重复打印同一实例，只保留一条连接记录。
    return;
  }

  endpoints.push_back(DiscoveredEndpoint{
      .kind = kind,
      .adb_path = adb_path,
      .serial = std::move(serial),
      .host = std::move(host),
      .port = port,
  });
}

// 从 MuMuManager 输出中兼容解析 adb_host/adb_host_ip 与字段顺序的多种 JSON 形式。
auto parse_mumu_endpoints(const std::filesystem::path& adb_path, std::string_view output)
    -> std::vector<DiscoveredEndpoint> {
  std::vector<DiscoveredEndpoint> endpoints;
  const std::string output_copy(output);

  const std::array<std::regex, 4> patterns = {
      std::regex(R"json("adb_host"\s*:\s*"([^"]+)"\s*,\s*"adb_port"\s*:\s*(\d+))json",
                 std::regex_constants::icase),
      std::regex(R"json("adb_port"\s*:\s*(\d+)\s*,\s*"adb_host"\s*:\s*"([^"]+)")json",
                 std::regex_constants::icase),
      std::regex(R"json("adb_host_ip"\s*:\s*"([^"]+)"\s*,\s*"adb_port"\s*:\s*(\d+))json",
                 std::regex_constants::icase),
      std::regex(R"json("adb_port"\s*:\s*(\d+)\s*,\s*"adb_host_ip"\s*:\s*"([^"]+)")json",
                 std::regex_constants::icase),
  };

  for (std::size_t pattern_index = 0; pattern_index < patterns.size(); ++pattern_index) {
    for (std::sregex_iterator it(output_copy.begin(), output_copy.end(), patterns[pattern_index]),
         end;
         it != end; ++it) {
      const auto& match = *it;
      if (match.size() < 3) {
        continue;
      }

      const auto host = pattern_index % 2 == 0 ? match[1].str() : match[2].str();
      const auto port_text = pattern_index % 2 == 0 ? match[2].str() : match[1].str();
      const auto port = parse_integer(port_text);
      if (port) {
        // 端口可解析时交给统一入口校验范围并完成去重。
        add_endpoint(endpoints, EmulatorKind::MuMu, adb_path, host, *port);
      }
    }
  }

  return endpoints;
}

// 从 LDConsole list2 的 CSV 输出中按虚拟机索引推导 emulator-xxxx serial。
auto parse_ld_endpoints(const std::filesystem::path& adb_path, std::string_view output)
    -> std::vector<DiscoveredEndpoint> {
  std::vector<DiscoveredEndpoint> endpoints;
  std::istringstream stream{std::string(output)};
  std::string line;

  while (std::getline(stream, line)) {
    // LDConsole 每行代表一个实例，字段不足时跳过表头或异常输出。
    const auto fields = split_csv(line);
    if (fields.size() < 7) {
      continue;
    }

    const auto index = parse_integer(fields[0]);
    const auto vbox_process_id = parse_integer(fields[6]);
    if (!index || !vbox_process_id || *index < 0 || *vbox_process_id <= 0) {
      continue;
    }
    if (*index > (std::numeric_limits<int>::max() - 5554) / 2) {
      continue;
    }

    // LDPlayer 的 ADB serial 从实例索引按 5554、5556……规律生成。
    add_endpoint(endpoints, EmulatorKind::LDPlayer, adb_path, "", 0,
                 std::format("emulator-{}", 5554 + *index * 2));
  }

  return endpoints;
}

// 调用候选模拟器的管理工具或 ADB，并把输出转换为可连接的 endpoint 列表。
auto discover_endpoints(const EmulatorCandidate& candidate)
    -> std::expected<std::vector<DiscoveredEndpoint>, std::string> {
  if (candidate.kind == EmulatorKind::BlueStacks) {
    auto devices_result = adb::list_devices(AdbConnectionConfig{.executable = candidate.adb_path});
    if (!devices_result) {
      return std::unexpected("BlueStacks ADB device discovery failed: " + devices_result.error());
    }

    std::vector<DiscoveredEndpoint> endpoints;
    for (const auto& device : devices_result.value()) {
      if (device.state != "device") {
        continue;
      }
      // BlueStacks 已将可用实例登记在 ADB 列表中，serial 就是唯一连接目标。
      add_endpoint(endpoints, EmulatorKind::BlueStacks, candidate.adb_path, {}, 0, device.serial);
    }
    if (endpoints.empty()) {
      return std::unexpected(
          "BlueStacks ADB device discovery found no ready devices. Ensure BlueStacks is running "
          "and ADB is enabled in its settings.");
    }
    return endpoints;
  }

  if (candidate.management_tool.empty()) {
    return std::vector<DiscoveredEndpoint>{};
  }

  if (candidate.kind == EmulatorKind::MuMu) {
    // MuMuManager 的 adb --vmindex all 会返回所有虚拟机的 host/port。
    auto result = run_management_tool(candidate.management_tool, {L"adb", L"--vmindex", L"all"},
                                      "MuMuManager");
    if (!result) {
      return std::unexpected(result.error());
    }
    return parse_mumu_endpoints(candidate.adb_path, result->stdout_data);
  }

  auto result = run_management_tool(candidate.management_tool, {L"list2"}, "LDConsole");
  if (!result) {
    return std::unexpected(result.error());
  }
  return parse_ld_endpoints(candidate.adb_path, result->stdout_data);
}

// 把发现结果写回连接配置，供后续 adb 客户端直接使用。
auto set_discovered_endpoint(AdbConnectionConfig& config, const DiscoveredEndpoint& endpoint)
    -> void {
  config.executable = endpoint.adb_path;
  config.serial = endpoint.serial;
  if (!endpoint.host.empty()) {
    // LDPlayer 的 endpoint 只有 serial，不覆盖原有 host/port 默认值。
    config.host = endpoint.host;
    config.port = endpoint.port;
  }
}

// 生成多个模拟器实例同时存在时的可操作错误提示。
auto make_multiple_devices_error(const std::vector<DiscoveredEndpoint>& endpoints) -> std::string {
  std::string error =
      "Multiple emulator instances were found. Set the device serial "
      "explicitly: ";
  for (std::size_t index = 0; index < endpoints.size(); ++index) {
    if (index != 0) {
      error += ", ";
    }
    error += endpoints[index].serial;
  }
  return error;
}

auto append_discovery_failures(std::string error, const std::vector<std::string>& failures)
    -> std::string {
  if (failures.empty()) {
    return error;
  }
  error += " Automatic discovery diagnostics: ";
  for (std::size_t index = 0; index < failures.size(); ++index) {
    if (index != 0) {
      error += "; ";
    }
    error += failures[index];
  }
  return error;
}

// 尝试解析 ADB 可执行文件：如果配置了就用配置的；否则看候选模拟器；再否则看系统 PATH。
auto resolve_any_adb_executable(const std::filesystem::path& configured_executable,
                                const std::vector<EmulatorCandidate>& candidates)
    -> std::expected<std::filesystem::path, std::string> {
  if (!configured_executable.empty()) {
    const auto configured_path = path_to_utf8(configured_executable);
    return adb::resolve_executable(configured_path);
  }

  if (!candidates.empty()) {
    return candidates.front().adb_path;
  }

  auto path_result = adb::resolve_executable("adb.exe");
  if (path_result) {
    return path_result.value();
  }

  return std::unexpected("message.adb_no_emulator_found");
}

}  // namespace

// 解析用户配置或自动发现模拟器，最终补齐可执行文件和设备序列号。
auto resolve_connection(AdbConnectionConfig config)
    -> std::expected<AdbConnectionConfig, std::string> {
  if (!config.executable.empty()) {
    // 用户指定 ADB 时完全信任其来源，不扫描其他模拟器安装目录。
    const auto configured_path = path_to_utf8(config.executable);
    auto resolved = adb::resolve_executable(configured_path);
    if (!resolved) {
      return std::unexpected(resolved.error());
    }
    config.executable = resolved.value();

    if (!config.serial.empty()) {
      return config;
    }

    // 未指定 serial 时，若刚好只有 1 台已连接设备，自动选中它。
    auto devices_result = adb::list_devices(config);
    if (devices_result) {
      std::vector<AdbDevice> ready_devices;
      for (const auto& dev : devices_result.value()) {
        if (dev.state == "device") {
          ready_devices.push_back(dev);
        }
      }
      if (ready_devices.size() == 1) {
        config.serial = ready_devices.front().serial;
        return config;
      }
      if (ready_devices.size() > 1) {
        std::vector<DiscoveredEndpoint> endpoints;
        for (const auto& dev : ready_devices) {
          endpoints.push_back(DiscoveredEndpoint{.serial = dev.serial});
        }
        return std::unexpected(make_multiple_devices_error(endpoints));
      }
    }

    return config;
  }

  const auto candidates = find_running_emulators();
  const bool has_bluestacks_candidate =
      std::ranges::any_of(candidates, [](const EmulatorCandidate& candidate) {
        return candidate.kind == EmulatorKind::BlueStacks;
      });
  std::vector<DiscoveredEndpoint> endpoints;
  std::vector<std::string> discovery_failures;
  for (const auto& candidate : candidates) {
    // 每个模拟器候选独立发现 endpoint，失败的候选不影响其他候选。
    auto candidate_endpoints = discover_endpoints(candidate);
    if (!candidate_endpoints) {
      discovery_failures.push_back(candidate_endpoints.error());
      continue;
    }
    auto& discovered = candidate_endpoints.value();
    endpoints.insert(endpoints.end(), std::make_move_iterator(discovered.begin()),
                     std::make_move_iterator(discovered.end()));
  }

  if (!config.serial.empty()) {
    // 有明确 serial 时优先在自动发现结果中匹配它。
    const auto matching_endpoint = std::ranges::find_if(
        endpoints,
        [&config](const DiscoveredEndpoint& endpoint) { return endpoint.serial == config.serial; });
    if (matching_endpoint != endpoints.end()) {
      set_discovered_endpoint(config, *matching_endpoint);
      return config;
    }

    // 未在模拟器 endpoints 中匹配到，尝试使用候选 ADB 或系统 PATH ADB 连接该设备（支持 USB
    // 真机或无线调试）。
    auto adb_executable = resolve_any_adb_executable(config.executable, candidates);
    if (adb_executable) {
      config.executable = *adb_executable;
      return config;
    }

    if (has_bluestacks_candidate) {
      return std::unexpected(append_discovery_failures(
          std::format("Configured ADB device serial was not found among automatically discovered "
                      "devices: {}",
                      config.serial),
          discovery_failures));
    }
  } else if (!endpoints.empty()) {
    const auto configured_endpoint = std::format("{}:{}", config.host, config.port);
    // 优先保留设置中的默认 host/port，保证用户选择不会被自动发现覆盖。
    const auto matching_configured_endpoint =
        std::ranges::find_if(endpoints, [&configured_endpoint](const DiscoveredEndpoint& endpoint) {
          return endpoint.serial == configured_endpoint;
        });
    if (matching_configured_endpoint != endpoints.end()) {
      set_discovered_endpoint(config, *matching_configured_endpoint);
      return config;
    }
    if (endpoints.size() == 1) {
      // 只有一个实例时可以安全地自动选择它。
      set_discovered_endpoint(config, endpoints.front());
      return config;
    }
    // 多个实例且用户没有选择时必须让用户明确指定目标。
    return std::unexpected(make_multiple_devices_error(endpoints));
  }

  if (has_bluestacks_candidate && !discovery_failures.empty()) {
    // BlueStacks 通过设备列表发现实例，失败时不能退回默认 host/port。
    return std::unexpected(
        append_discovery_failures("Automatic emulator discovery failed", discovery_failures));
  }

  auto resolved_adb = resolve_any_adb_executable(config.executable, candidates);
  if (resolved_adb) {
    config.executable = *resolved_adb;
    // 尝试直接查看 ADB 设备列表中是否只有 1 个就绪设备
    auto devices_result = adb::list_devices(config);
    if (devices_result) {
      std::vector<AdbDevice> ready_devices;
      for (const auto& dev : devices_result.value()) {
        if (dev.state == "device") {
          ready_devices.push_back(dev);
        }
      }
      if (ready_devices.size() == 1) {
        config.serial = ready_devices.front().serial;
        return config;
      }
      if (ready_devices.size() > 1) {
        return std::unexpected("message.adb_multiple_emulators_found");
      }
    }
    if (candidates.size() == 1) {
      return config;
    }
    if (candidates.size() > 1) {
      return std::unexpected("message.adb_multiple_emulators_found");
    }
  }

  return std::unexpected("message.adb_no_emulator_found");
}

// 扫描并发现所有当前可用的 Android 设备（包括运行中模拟器与已识别的真机）。
auto discover_all_devices(AdbConnectionConfig config)
    -> std::expected<std::vector<DiscoveredAdbDevice>, std::string> {
  const auto candidates = find_running_emulators();
  auto adb_executable = resolve_any_adb_executable(config.executable, candidates);
  if (!adb_executable) {
    return std::vector<DiscoveredAdbDevice>{};
  }
  config.executable = *adb_executable;

  // 收集所有正在运行的模拟器候选端点
  std::vector<DiscoveredEndpoint> endpoints;
  for (const auto& candidate : candidates) {
    auto candidate_endpoints = discover_endpoints(candidate);
    if (!candidate_endpoints) {
      continue;
    }
    endpoints.insert(endpoints.end(), candidate_endpoints->begin(), candidate_endpoints->end());
  }

  // 对发现的网络模拟器端点尝试快速预连接，确保它们在 adb devices 中列出
  for (const auto& endpoint : endpoints) {
    if (!endpoint.serial.empty() && endpoint.serial.find(':') != std::string::npos) {
      static_cast<void>(
          adb::connect_endpoint(config, endpoint.serial, std::chrono::milliseconds(800)));
    }
  }

  // 执行 adb devices -l 获取完整设备列表
  auto devices_result = adb::list_devices(config);
  std::vector<DiscoveredAdbDevice> result;

  auto classify_device = [&](const AdbDevice& device) -> std::pair<std::string, bool> {
    for (const auto& ep : endpoints) {
      if (ep.serial == device.serial) {
        switch (ep.kind) {
          case EmulatorKind::MuMu:
            return {"mumu", true};
          case EmulatorKind::LDPlayer:
            return {"ldplayer", true};
          case EmulatorKind::BlueStacks:
            return {"bluestacks", true};
        }
      }
    }
    std::string lower_model = device.model;
    std::ranges::transform(lower_model, lower_model.begin(), [](char character) {
      return static_cast<char>(std::tolower(character));
    });
    if (lower_model.find("mumu") != std::string::npos) {
      return {"mumu", true};
    }
    if (lower_model.find("emulator") != std::string::npos ||
        lower_model.find("vbox") != std::string::npos ||
        lower_model.find("goldfish") != std::string::npos ||
        lower_model.find("sdk") != std::string::npos) {
      return {"emulator", true};
    }
    return {"device", false};
  };

  if (devices_result) {
    for (const auto& dev : devices_result.value()) {
      const auto [kind, is_emulator] = classify_device(dev);
      result.push_back(DiscoveredAdbDevice{
          .serial = dev.serial,
          .kind = kind,
          .model = dev.model,
          .state = dev.state,
          .is_emulator = is_emulator,
      });
    }
  }

  // 若部分探测到的模拟器仍未出现在 adb devices 列表中，作为未就绪项补齐
  for (const auto& ep : endpoints) {
    if (std::ranges::none_of(
            result, [&ep](const DiscoveredAdbDevice& dev) { return dev.serial == ep.serial; })) {
      std::string kind = "emulator";
      switch (ep.kind) {
        case EmulatorKind::MuMu:
          kind = "mumu";
          break;
        case EmulatorKind::LDPlayer:
          kind = "ldplayer";
          break;
        case EmulatorKind::BlueStacks:
          kind = "bluestacks";
          break;
      }
      result.push_back(DiscoveredAdbDevice{
          .serial = ep.serial,
          .kind = kind,
          .model = "",
          .state = "offline",
          .is_emulator = true,
      });
    }
  }

  return result;
}

}  // namespace features::adb_mode::device_finder
