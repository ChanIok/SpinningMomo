module;

module Utils.Logger;

import std;
import Utils.Path;

// Windows API 声明（避免包含 windows.h）
extern "C" {
__declspec(dllimport) void* __stdcall CreateFileW(
    const wchar_t* lpFileName, unsigned long dwDesiredAccess, unsigned long dwShareMode,
    void* lpSecurityAttributes, unsigned long dwCreationDisposition,
    unsigned long dwFlagsAndAttributes, void* hTemplateFile);

__declspec(dllimport) int __stdcall WriteFile(void* hFile, const void* lpBuffer,
                                              unsigned long nNumberOfBytesToWrite,
                                              unsigned long* lpNumberOfBytesWritten,
                                              void* lpOverlapped);

__declspec(dllimport) int __stdcall FlushFileBuffers(void* hFile);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
__declspec(dllimport) unsigned long __stdcall GetLastError();
__declspec(dllimport) void __stdcall OutputDebugStringW(const wchar_t* lpOutputString);

// 字符转换 API
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int CodePage,
                                                        unsigned long dwFlags,
                                                        const char* lpMultiByteStr, int cbMultiByte,
                                                        wchar_t* lpWideCharStr, int cchWideChar);
}

// Windows API 常量
constexpr unsigned long GENERIC_WRITE = 0x40000000;
constexpr unsigned long FILE_SHARE_READ = 0x00000001;
constexpr unsigned long CREATE_ALWAYS = 2;
constexpr unsigned long OPEN_ALWAYS = 4;
constexpr unsigned long FILE_ATTRIBUTE_NORMAL = 0x80;
constexpr unsigned int CP_UTF8 = 65001;

// Windows INVALID_HANDLE_VALUE 定义
inline void* get_invalid_handle_value() { return reinterpret_cast<void*>(-1); }

// UTF-8 到 UTF-16 转换辅助函数（使用 Windows API）
std::wstring utf8_to_utf16(const std::string& utf8_str) {
  if (utf8_str.empty()) return {};

  // 先获取需要的缓冲区大小
  int wide_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(),
                                      static_cast<int>(utf8_str.length()), nullptr, 0);
  if (wide_size <= 0) return {};

  // 分配缓冲区并转换
  std::wstring wide_str(wide_size, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), static_cast<int>(utf8_str.length()),
                      &wide_str[0], wide_size);

  return wide_str;
}

// 获取单例实现
Logger::LoggerImpl& Logger::get_impl() {
  static LoggerImpl impl{.log_file_handle = get_invalid_handle_value()};
  return impl;
}

// 初始化日志系统
std::expected<void, LoggerError> Logger::Initialize() {
  auto& impl = get_impl();
  std::lock_guard<std::mutex> lock(impl.mutex);

  if (impl.initialized.load(std::memory_order_acquire)) {
    return std::unexpected(LoggerError::ALREADY_INITIALIZED);
  }

  // 获取程序所在的目录
  auto exec_dir_result = Utils::Path::GetExecutableDirectory();
  if (!exec_dir_result) {
    return std::unexpected(LoggerError::MODULE_PATH_FAILED);
  }

  // 日志文件放在程序所在目录下
  auto log_file_path = Utils::Path::Combine(exec_dir_result.value(), "app.log");

  // 转换为 UTF-16 路径
  std::wstring wide_path = utf8_to_utf16(log_file_path.string());

  // 使用 Windows API 创建文件
  impl.log_file_handle = CreateFileW(wide_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                     CREATE_ALWAYS,  // 总是创建新文件（覆盖旧的）
                                     FILE_ATTRIBUTE_NORMAL, nullptr);

  if (impl.log_file_handle == get_invalid_handle_value()) {
    return std::unexpected(LoggerError::FILE_OPEN_FAILED);
  }

  impl.initialized.store(true, std::memory_order_release);

  // 记录初始日志
  const std::string start_message =
      std::format("[{}] ---------- Application Started ----------\n", get_timestamp());

  unsigned long bytes_written;
  WriteFile(impl.log_file_handle, start_message.c_str(),
            static_cast<unsigned long>(start_message.length()), &bytes_written, nullptr);
  FlushFileBuffers(impl.log_file_handle);

  // 设置默认日志级别
#ifdef NDEBUG
  impl.current_log_level.store(LogLevel::INFO, std::memory_order_relaxed);
#else
  impl.current_log_level.store(LogLevel::DEBUG, std::memory_order_relaxed);
#endif

  return {};
}

// 关闭日志系统
void Logger::Shutdown() {
  auto& impl = get_impl();
  std::lock_guard<std::mutex> lock(impl.mutex);

  if (impl.log_file_handle != get_invalid_handle_value()) {
    const std::string end_message =
        std::format("[{}] ---------- Application Ended ----------\n", get_timestamp());

    unsigned long bytes_written;
    WriteFile(impl.log_file_handle, end_message.c_str(),
              static_cast<unsigned long>(end_message.length()), &bytes_written, nullptr);
    FlushFileBuffers(impl.log_file_handle);
    CloseHandle(impl.log_file_handle);
    impl.log_file_handle = get_invalid_handle_value();
  }

  impl.initialized.store(false, std::memory_order_release);
}

// 设置日志级别
void Logger::SetLogLevel(LogLevel level) noexcept {
  auto& impl = get_impl();
  impl.current_log_level.store(level, std::memory_order_relaxed);
}

// 获取当前日志级别
LogLevel Logger::GetLogLevel() noexcept {
  auto& impl = get_impl();
  return impl.current_log_level.load(std::memory_order_relaxed);
}

// 检查是否应该记录日志
bool Logger::should_log(LogLevel level) noexcept {
  auto& impl = get_impl();

  // 如果未初始化，尝试初始化
  if (!impl.initialized.load(std::memory_order_acquire)) {
    auto result = Initialize();
    if (!result) {
      return false;  // 初始化失败，忽略日志
    }
  }

  // 如果日志级别低于当前设置的级别，则忽略
  return level >= impl.current_log_level.load(std::memory_order_relaxed);
}

// 获取当前时间的字符串表示
std::string Logger::get_timestamp() {
  const auto now = std::chrono::system_clock::now();
  return std::format("{:%Y-%m-%d %H:%M:%S}", now);
}

// 内部写入日志的实现
void Logger::write_log(LogLevel level, const std::string& message) const {
  auto& impl = get_impl();
  const std::string timestamp = get_timestamp();
  const std::string_view level_str = get_level_string(level);

  // 从绝对路径中提取文件名
  std::string_view filename = loc_.file_name();
  if (auto pos = filename.find_last_of("/\\"); pos != std::string_view::npos) {
    filename = filename.substr(pos + 1);
  }

  // 构建完整的日志消息，只包含文件名和行号
  const std::string log_message =
      std::format("{} [{}] [{}:{}] {}\n", timestamp, level_str, filename, loc_.line(), message);

  // 线程安全地写入日志
  {
    std::lock_guard<std::mutex> lock(impl.mutex);

    // 写入文件
    if (impl.log_file_handle != get_invalid_handle_value()) {
      unsigned long bytes_written;
      WriteFile(impl.log_file_handle, log_message.c_str(),
                static_cast<unsigned long>(log_message.length()), &bytes_written, nullptr);
      FlushFileBuffers(impl.log_file_handle);
    }

    // 也输出到控制台（用于调试）
    std::wstring wide_message = utf8_to_utf16(log_message);
    OutputDebugStringW(wide_message.c_str());
  }
}

// 简单字符串重载实现
void Logger::debug(std::string_view msg) const {
  if (should_log(LogLevel::DEBUG)) {
    write_log(LogLevel::DEBUG, std::string(msg));
  }
}

void Logger::info(std::string_view msg) const {
  if (should_log(LogLevel::INFO)) {
    write_log(LogLevel::INFO, std::string(msg));
  }
}

void Logger::warn(std::string_view msg) const {
  if (should_log(LogLevel::WARN)) {
    write_log(LogLevel::WARN, std::string(msg));
  }
}

void Logger::error(std::string_view msg) const {
  if (should_log(LogLevel::ERROR)) {
    write_log(LogLevel::ERROR, std::string(msg));
  }
}