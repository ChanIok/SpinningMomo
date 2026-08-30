#include "utils/process/process.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace utils::process::detail {

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
      ++backslash_count;
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

  // 参数末尾的反斜杠需要翻倍，否则会转义包住参数的结束引号。
  quoted.append(backslash_count * 2, L'\\');
  quoted.push_back(L'\"');
  return quoted;
}

auto build_command_line(const std::filesystem::path& executable,
                        const std::vector<std::wstring>& arguments) -> std::wstring {
  std::wstring command_line = quote_argument(executable.wstring());
  for (const auto& argument : arguments) {
    command_line.push_back(L' ');
    command_line += quote_argument(argument);
  }
  return command_line;
}

auto close_handle(HANDLE& handle) -> void {
  if (handle && handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);
  }
  handle = nullptr;
}

auto read_pipe(HANDLE pipe, std::string& output) -> void {
  std::array<char, 64 * 1024> buffer{};
  DWORD bytes_read = 0;

  while (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr) &&
         bytes_read > 0) {
    output.append(buffer.data(), bytes_read);
  }
}

auto timeout_to_dword(std::chrono::milliseconds timeout) -> DWORD {
  if (timeout.count() <= 0) {
    return 0;
  }

  constexpr auto kMaxWait = static_cast<std::int64_t>(INFINITE - 1);
  const auto count = std::min<std::int64_t>(timeout.count(), kMaxWait);
  return static_cast<DWORD>(count);
}

}  // namespace utils::process::detail

namespace utils::process {

auto run(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
         const RunOptions& options) -> std::expected<CommandResult, std::string> {
  if (executable.empty()) {
    return std::unexpected("Process executable cannot be empty");
  }

  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = TRUE;

  HANDLE stdout_read = nullptr;
  HANDLE stdout_write = nullptr;
  HANDLE stderr_read = nullptr;
  HANDLE stderr_write = nullptr;
  HANDLE stdin_nul = nullptr;

  auto cleanup_handles = [&]() {
    detail::close_handle(stdout_read);
    detail::close_handle(stdout_write);
    detail::close_handle(stderr_read);
    detail::close_handle(stderr_write);
    detail::close_handle(stdin_nul);
  };

  if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0) ||
      !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to create stdout pipe: " + std::to_string(error));
  }

  if (!CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0) ||
      !SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0)) {
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to create stderr pipe: " + std::to_string(error));
  }

  stdin_nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &security_attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (!stdin_nul || stdin_nul == INVALID_HANDLE_VALUE) {
    stdin_nul = nullptr;
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to open NUL for process stdin: " + std::to_string(error));
  }

  const auto command_line = detail::build_command_line(executable, arguments);
  auto mutable_command_line = command_line;

  // 只把标准 I/O 句柄传给子进程，避免监听 socket 等无关句柄被全局继承。
  std::array inherited_handles{stdin_nul, stdout_write, stderr_write};
  SIZE_T attribute_list_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_list_size);
  if (attribute_list_size == 0) {
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to size process attribute list: " + std::to_string(error));
  }

  std::vector<std::byte> attribute_storage(attribute_list_size);
  auto* attribute_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_list_size)) {
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to initialize process attribute list: " + std::to_string(error));
  }

  if (!UpdateProcThreadAttribute(attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherited_handles.data(),
                                 inherited_handles.size() * sizeof(HANDLE), nullptr, nullptr)) {
    const auto error = GetLastError();
    DeleteProcThreadAttributeList(attribute_list);
    cleanup_handles();
    return std::unexpected("Failed to configure inherited process handles: " +
                           std::to_string(error));
  }

  STARTUPINFOEXW startup_info{};
  startup_info.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup_info.StartupInfo.hStdInput = stdin_nul;
  startup_info.StartupInfo.hStdOutput = stdout_write;
  startup_info.StartupInfo.hStdError = stderr_write;
  startup_info.lpAttributeList = attribute_list;

  PROCESS_INFORMATION process_info{};
  std::wstring working_directory;
  LPCWSTR working_directory_ptr = nullptr;
  if (!options.working_directory.empty()) {
    working_directory = options.working_directory.wstring();
    working_directory_ptr = working_directory.c_str();
  }

  const BOOL created = CreateProcessW(
      executable.wstring().c_str(), mutable_command_line.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, nullptr,
      working_directory_ptr, &startup_info.StartupInfo, &process_info);

  DeleteProcThreadAttributeList(attribute_list);

  // 父进程不应继续持有写端，否则读线程永远等不到 EOF。
  detail::close_handle(stdout_write);
  detail::close_handle(stderr_write);
  detail::close_handle(stdin_nul);

  if (!created) {
    const auto error = GetLastError();
    cleanup_handles();
    return std::unexpected("Failed to start process: " + std::to_string(error));
  }

  detail::close_handle(process_info.hThread);

  CommandResult result;
  std::thread stdout_thread([pipe = stdout_read, &output = result.stdout_data]() mutable {
    detail::read_pipe(pipe, output);
    detail::close_handle(pipe);
  });
  stdout_read = nullptr;

  std::thread stderr_thread([pipe = stderr_read, &output = result.stderr_data]() mutable {
    detail::read_pipe(pipe, output);
    detail::close_handle(pipe);
  });
  stderr_read = nullptr;

  const DWORD wait_timeout = detail::timeout_to_dword(options.timeout);
  const DWORD wait_result = WaitForSingleObject(process_info.hProcess, wait_timeout);
  if (wait_result == WAIT_TIMEOUT) {
    result.timed_out = true;
    TerminateProcess(process_info.hProcess, ERROR_TIMEOUT);
    WaitForSingleObject(process_info.hProcess, INFINITE);
  } else if (wait_result == WAIT_FAILED) {
    const auto error = GetLastError();
    TerminateProcess(process_info.hProcess, ERROR_FUNCTION_FAILED);
    WaitForSingleObject(process_info.hProcess, INFINITE);
    detail::close_handle(process_info.hProcess);
    stdout_thread.join();
    stderr_thread.join();
    return std::unexpected("Failed while waiting for process: " + std::to_string(error));
  }

  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) {
    const auto error = GetLastError();
    detail::close_handle(process_info.hProcess);
    stdout_thread.join();
    stderr_thread.join();
    return std::unexpected("Failed to read process exit code: " + std::to_string(error));
  }

  result.exit_code = static_cast<std::uint32_t>(exit_code);
  detail::close_handle(process_info.hProcess);
  stdout_thread.join();
  stderr_thread.join();

  return result;
}

}  // namespace utils::process
