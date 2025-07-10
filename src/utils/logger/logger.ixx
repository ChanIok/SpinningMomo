module;

#include <spdlog/spdlog.h>

export module Utils.Logger;

import std;

export using LoggerError = std::string;

namespace Utils::Logging {

// 日志管理函数
export auto initialize() -> std::expected<void, LoggerError>;
export auto shutdown() -> void;
export auto flush() -> void;

}  // namespace Utils::Logging

// Logger类 - 使用构造函数捕获source_location
export class Logger {
 public:
  Logger(std::source_location loc = std::source_location::current());

  // 格式化日志函数
  template <typename... Args>
  auto trace(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto debug(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto info(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto warn(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto error(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::err, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto critical(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::critical, fmt, std::forward<Args>(args)...);
  }

  // 简单字符串日志函数
  auto trace(std::string_view msg) const -> void;
  auto debug(std::string_view msg) const -> void;
  auto info(std::string_view msg) const -> void;
  auto warn(std::string_view msg) const -> void;
  auto error(std::string_view msg) const -> void;
  auto critical(std::string_view msg) const -> void;

 private:
  std::source_location loc_;
};
