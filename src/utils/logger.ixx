module;

export module Utils.Logger;

import std;
import Utils.Path;

// 日志级别
export enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

// 日志初始化结果
export enum class LoggerError {
  SUCCESS,
  MODULE_PATH_FAILED,
  FILE_OPEN_FAILED,
  ALREADY_INITIALIZED
};

// 现代化的日志记录类
export class Logger {
 public:
  // 构造函数捕获调用位置
  Logger(std::source_location loc = std::source_location::current()) : loc_(loc) {}

  // 各级别日志方法
  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args) const {
    log_with_level<LogLevel::DEBUG>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args) const {
    log_with_level<LogLevel::INFO>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args) const {
    log_with_level<LogLevel::WARN>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args) const {
    log_with_level<LogLevel::ERROR>(fmt, std::forward<Args>(args)...);
  }

  // 简单字符串重载
  void debug(std::string_view msg) const;
  void info(std::string_view msg) const;
  void warn(std::string_view msg) const;
  void error(std::string_view msg) const;

  // 静态管理方法
  static std::expected<void, LoggerError> Initialize();
  static void SetLogLevel(LogLevel level) noexcept;
  static LogLevel GetLogLevel() noexcept;
  static void Shutdown();

 private:
  std::source_location loc_;

  // 统一的日志实现
  template <LogLevel Level, typename... Args>
  void log_with_level(std::format_string<Args...> fmt, Args&&... args) const {
    if (should_log(Level)) {
      std::string message = std::format(fmt, std::forward<Args>(args)...);
      write_log(Level, message);
    }
  }

  // 内部实现
  static bool should_log(LogLevel level) noexcept;
  void write_log(LogLevel level, const std::string& message) const;
  static std::string get_timestamp();
  static constexpr std::string_view get_level_string(LogLevel level) noexcept;

  // 单例数据
  struct LoggerImpl {
    std::mutex mutex;
    std::ofstream log_file;
    std::atomic<bool> initialized{false};
    std::atomic<LogLevel> current_log_level{LogLevel::INFO};
  };
  static LoggerImpl& get_impl();
};

// constexpr 日志级别字符串映射
constexpr std::string_view Logger::get_level_string(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARN:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}