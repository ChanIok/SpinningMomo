module;

module Utils.Logger;

import std;
import Utils.Path;
import Vendor.Windows;

// 获取单例实现
Logger::LoggerImpl& Logger::get_impl() {
  static LoggerImpl impl;
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
  impl.log_file.open(log_file_path, std::ios::out | std::ios::trunc);
  if (!impl.log_file.is_open()) {
    return std::unexpected(LoggerError::FILE_OPEN_FAILED);
  }

  impl.initialized.store(true, std::memory_order_release);

  // 记录初始日志
  const std::string start_message =
      std::format("[{}] ---------- Application Started ----------", get_timestamp());
  impl.log_file << start_message << std::endl;
  impl.log_file.flush();

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

  if (impl.log_file.is_open()) {
    const std::string end_message =
        std::format("{} ---------- Application Ended ----------", get_timestamp());
    impl.log_file << end_message << std::endl;
    impl.log_file.close();
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
      std::format("{} [{}] [{}:{}] {}", timestamp, level_str, filename, loc_.line(), message);

  // 线程安全地写入日志
  {
    std::lock_guard<std::mutex> lock(impl.mutex);

    // 写入文件
    if (impl.log_file.is_open()) {
      impl.log_file << log_message << std::endl;
      impl.log_file.flush();
    }

    // 也输出到控制台（用于调试）
    std::wstring wide_message(log_message.begin(), log_message.end());
    wide_message += L"\n";
    Vendor::Windows::OutputDebugStringW(wide_message.c_str());
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