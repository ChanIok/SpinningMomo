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

namespace {

auto create_null_handle() -> std::expected<wil::unique_handle, std::string> {
  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  attributes.bInheritHandle = TRUE;
  HANDLE handle =
      CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (!handle || handle == INVALID_HANDLE_VALUE) {
    return std::unexpected("Failed to open NUL for child process: " +
                           std::to_string(GetLastError()));
  }
  return wil::unique_handle(handle);
}

auto create_output_file(const std::filesystem::path& path)
    -> std::expected<wil::unique_handle, std::string> {
  if (path.empty()) {
    return std::unexpected("Process output file path cannot be empty");
  }

  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  attributes.bInheritHandle = TRUE;
  HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &attributes,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (!handle || handle == INVALID_HANDLE_VALUE) {
    return std::unexpected("Failed to create process output file: " +
                           std::to_string(GetLastError()));
  }
  return wil::unique_handle(handle);
}

}  // namespace

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

// 启动长期后台子进程：准备标准 I/O 句柄 → 配置属性列表继承白名单 → 创建无窗口子进程
auto spawn(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
           const SpawnOptions& options) -> std::expected<ChildProcess, std::string> {
  if (executable.empty()) {
    return std::unexpected("Process executable cannot be empty");
  }

  // 为 stdin 和 stdout 打开 NUL 句柄，保持后台静默
  auto null_result = create_null_handle();
  if (!null_result) {
    return std::unexpected(null_result.error());
  }
  auto null_handle = std::move(null_result.value());

  // 配置 stderr 目标：指定文件时重定向至文件，否则丢弃至 NUL
  wil::unique_handle stderr_handle;
  if (options.stderr_file.empty()) {
    auto stderr_result = create_null_handle();
    if (!stderr_result) {
      return std::unexpected(stderr_result.error());
    }
    stderr_handle = std::move(stderr_result.value());
  } else {
    auto stderr_result = create_output_file(options.stderr_file);
    if (!stderr_result) {
      return std::unexpected(stderr_result.error());
    }
    stderr_handle = std::move(stderr_result.value());
  }

  // 构造 Windows 命令行字符串
  const auto command_line = detail::build_command_line(executable, arguments);
  auto mutable_command_line = command_line;

  // 初始化进程属性列表，仅白名单继承必要的 I/O 句柄，避免泄漏
  SIZE_T attribute_list_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_list_size);
  if (attribute_list_size == 0) {
    return std::unexpected("Failed to size child process attributes: " +
                           std::to_string(GetLastError()));
  }

  std::vector<std::byte> attribute_storage(attribute_list_size);
  auto* attribute_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_list_size)) {
    return std::unexpected("Failed to initialize child process attributes: " +
                           std::to_string(GetLastError()));
  }

  // 仅将 NUL 与 stderr 文件句柄加入继承列表
  HANDLE inherited_handles[] = {null_handle.get(), stderr_handle.get()};
  if (!UpdateProcThreadAttribute(attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherited_handles, sizeof(inherited_handles), nullptr, nullptr)) {
    const auto error = GetLastError();
    DeleteProcThreadAttributeList(attribute_list);
    return std::unexpected("Failed to configure child process handles: " + std::to_string(error));
  }

  // 配置扩展启动信息结构体
  STARTUPINFOEXW startup_info{};
  startup_info.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup_info.StartupInfo.hStdInput = null_handle.get();
  startup_info.StartupInfo.hStdOutput = null_handle.get();
  startup_info.StartupInfo.hStdError = stderr_handle.get();
  startup_info.lpAttributeList = attribute_list;

  PROCESS_INFORMATION process_info{};
  std::wstring working_directory;
  LPCWSTR working_directory_ptr = nullptr;
  if (!options.working_directory.empty()) {
    working_directory = options.working_directory.wstring();
    working_directory_ptr = working_directory.c_str();
  }

  // 创建无窗口后台进程
  const BOOL created = CreateProcessW(
      executable.wstring().c_str(), mutable_command_line.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, nullptr,
      working_directory_ptr, &startup_info.StartupInfo, &process_info);

  if (!created) {
    DeleteProcThreadAttributeList(attribute_list);
    return std::unexpected("Failed to start child process: " + std::to_string(GetLastError()));
  }

  // 释放属性列表与初始主线程句柄
  DeleteProcThreadAttributeList(attribute_list);
  CloseHandle(process_info.hThread);

  return ChildProcess{.process = wil::unique_handle(process_info.hProcess),
                      .stderr_file = options.stderr_file};
}

// 查询子进程当前退出状态码：检验句柄有效性 → 读取退出码并返回
auto get_exit_code(const ChildProcess& child) -> std::expected<std::uint32_t, std::string> {
  if (!child.process) {
    return std::unexpected("Child process is not running");
  }

  DWORD exit_code = 0;
  if (!GetExitCodeProcess(child.process.get(), &exit_code)) {
    return std::unexpected("Failed to read child process exit code: " +
                           std::to_string(GetLastError()));
  }
  return static_cast<std::uint32_t>(exit_code);
}

// 读取子进程重定向至文件的 stderr 诊断日志内容
auto read_stderr(const ChildProcess& child) -> std::expected<std::string, std::string> {
  if (child.stderr_file.empty()) {
    return std::string{};
  }

  // 打开并读取全部文件内容
  std::ifstream file(child.stderr_file, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open child process stderr file: " +
                           child.stderr_file.string());
  }

  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// 等待后台子进程结束：挂超时等待句柄信号 → 读取最终退出码
auto wait(ChildProcess& child, std::chrono::milliseconds timeout)
    -> std::expected<std::uint32_t, std::string> {
  if (!child.process) {
    return std::unexpected("Child process is not running");
  }

  // 等待进程句柄触发信号
  const auto wait_timeout = detail::timeout_to_dword(timeout);
  const auto wait_result = WaitForSingleObject(child.process.get(), wait_timeout);
  if (wait_result == WAIT_TIMEOUT) {
    return std::unexpected("Child process wait timed out");
  }
  if (wait_result == WAIT_FAILED) {
    return std::unexpected("Failed to wait for child process: " + std::to_string(GetLastError()));
  }

  // 获取进程最终退出码
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(child.process.get(), &exit_code)) {
    return std::unexpected("Failed to read child process exit code: " +
                           std::to_string(GetLastError()));
  }
  return static_cast<std::uint32_t>(exit_code);
}

// 强制终止后台子进程：检查活动状态 → 发送终止信号 → 阻塞等待其彻底退出
auto terminate(ChildProcess& child) -> std::expected<void, std::string> {
  if (!child.process) {
    return {};
  }

  // 检查目标进程是否仍在运行
  DWORD exit_code = STILL_ACTIVE;
  if (!GetExitCodeProcess(child.process.get(), &exit_code)) {
    const auto error = GetLastError();
    child.process.reset();
    return std::unexpected("Failed to inspect child process: " + std::to_string(error));
  }

  // 仍在运行则强制终止
  if (exit_code == STILL_ACTIVE && !TerminateProcess(child.process.get(), ERROR_PROCESS_ABORTED)) {
    const auto error = GetLastError();
    child.process.reset();
    return std::unexpected("Failed to terminate child process: " + std::to_string(error));
  }

  // 阻塞等待句柄完全释放并重置
  WaitForSingleObject(child.process.get(), INFINITE);
  child.process.reset();
  return {};
}

}  // namespace utils::process
