module;

module Utils.PowerShell;

import std;
import <windows.h>;

namespace Utils::PowerShell::Detail {

// 按 Windows CommandLineToArgvW 规则引用单个参数，保证空格和引号不改变参数边界。
auto quote_argument(std::wstring_view argument) -> std::wstring {
  if (argument.empty()) {
    return L"\"\"";
  }

  const bool needs_quotes = argument.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
  if (!needs_quotes) {
    return std::wstring(argument);
  }

  std::wstring quoted = L"\"";
  std::size_t backslash_count = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      backslash_count++;
      continue;
    }

    if (character == L'\"') {
      quoted.append(backslash_count * 2 + 1, L'\\');
      quoted.push_back(L'\"');
      backslash_count = 0;
      continue;
    }

    quoted.append(backslash_count, L'\\');
    backslash_count = 0;
    quoted.push_back(character);
  }

  // 末尾反斜杠要翻倍，避免它转义包住参数的结束引号。
  quoted.append(backslash_count * 2, L'\\');
  quoted.push_back(L'\"');
  return quoted;
}

// 定位系统自带 Windows PowerShell，避免从当前目录加载同名可执行文件。
auto get_powershell_path() -> std::expected<std::filesystem::path, std::string> {
  std::vector<wchar_t> windows_directory(MAX_PATH);
  const auto length =
      GetWindowsDirectoryW(windows_directory.data(), static_cast<UINT>(windows_directory.size()));
  if (length == 0) {
    return std::unexpected("Failed to locate Windows directory: " + std::to_string(GetLastError()));
  }
  if (length >= windows_directory.size()) {
    windows_directory.resize(static_cast<std::size_t>(length) + 1);
    if (GetWindowsDirectoryW(windows_directory.data(),
                             static_cast<UINT>(windows_directory.size())) == 0) {
      return std::unexpected("Failed to read Windows directory: " + std::to_string(GetLastError()));
    }
  }

  auto powershell_path = std::filesystem::path(windows_directory.data()) / L"System32" /
                         L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
  if (!std::filesystem::is_regular_file(powershell_path)) {
    return std::unexpected("Windows PowerShell was not found: " + powershell_path.string());
  }
  return powershell_path;
}

// 构造固定 PowerShell 启动参数，业务路径只作为独立参数传入脚本。
auto build_command_line(const std::filesystem::path& powershell_path,
                        const std::filesystem::path& script_path,
                        const std::vector<std::wstring>& arguments) -> std::wstring {
  std::wstring command_line = quote_argument(powershell_path.wstring());
  const std::array<std::wstring_view, 6> fixed_arguments = {
      L"-NoLogo", L"-NoProfile", L"-NonInteractive", L"-ExecutionPolicy", L"Bypass", L"-File",
  };

  for (const auto argument : fixed_arguments) {
    command_line += L" ";
    command_line += quote_argument(argument);
  }
  command_line += L" ";
  command_line += quote_argument(script_path.wstring());
  for (const auto& argument : arguments) {
    command_line += L" ";
    command_line += quote_argument(argument);
  }
  return command_line;
}

// 创建隐藏的 PowerShell 子进程，并按调用方要求决定是否等待完成。
auto start_process(const std::filesystem::path& script_path,
                   const std::vector<std::wstring>& arguments, bool wait_for_exit)
    -> std::expected<std::optional<std::uint32_t>, std::string> {
  auto powershell_path_result = get_powershell_path();
  if (!powershell_path_result) {
    return std::unexpected(powershell_path_result.error());
  }
  if (!std::filesystem::is_regular_file(script_path)) {
    return std::unexpected("PowerShell script does not exist: " + script_path.string());
  }

  auto command_line = build_command_line(*powershell_path_result, script_path, arguments);
  STARTUPINFOW startup_info{};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info{};

  // CREATE_NO_WINDOW 防止归档和重启脚本弹出控制台窗口。
  const BOOL created = CreateProcessW(
      powershell_path_result->c_str(), command_line.data(), nullptr, nullptr, FALSE,
      CREATE_NO_WINDOW, nullptr, script_path.parent_path().c_str(), &startup_info, &process_info);
  if (!created) {
    return std::unexpected("Failed to start Windows PowerShell: " + std::to_string(GetLastError()));
  }

  CloseHandle(process_info.hThread);
  if (!wait_for_exit) {
    CloseHandle(process_info.hProcess);
    return std::optional<std::uint32_t>{};
  }

  // 同步归档步骤必须拿到脚本退出码，才能决定是否发布最终 ZIP。
  const DWORD wait_result = WaitForSingleObject(process_info.hProcess, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    const auto error = GetLastError();
    CloseHandle(process_info.hProcess);
    return std::unexpected("Failed while waiting for Windows PowerShell: " + std::to_string(error));
  }

  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) {
    const auto error = GetLastError();
    CloseHandle(process_info.hProcess);
    return std::unexpected("Failed to read Windows PowerShell exit code: " + std::to_string(error));
  }

  CloseHandle(process_info.hProcess);
  return std::optional<std::uint32_t>{static_cast<std::uint32_t>(exit_code)};
}

}  // namespace Utils::PowerShell::Detail

namespace Utils::PowerShell {

// 同步执行 PowerShell 脚本并返回退出码，供调用方确认脚本结果。
auto run_script_and_wait(const std::filesystem::path& script_path,
                         const std::vector<std::wstring>& arguments)
    -> std::expected<std::uint32_t, std::string> {
  auto result = Detail::start_process(script_path, arguments, true);
  if (!result) {
    return std::unexpected(result.error());
  }
  if (!result->has_value()) {
    return std::unexpected("Windows PowerShell exited without a result");
  }
  return result->value();
}

// 后台启动 PowerShell 脚本，供需要等待当前进程退出的更新和恢复任务使用。
auto launch_script(const std::filesystem::path& script_path,
                   const std::vector<std::wstring>& arguments) -> std::expected<void, std::string> {
  auto result = Detail::start_process(script_path, arguments, false);
  if (!result) {
    return std::unexpected(result.error());
  }
  return {};
}

}  // namespace Utils::PowerShell
