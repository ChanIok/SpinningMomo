#pragma once

#include "vendor/std.hpp"

#include "vendor/spdlog.hpp"

namespace utils::logging {

// 日志管理函数
auto initialize(const std::optional<std::string>& configured_level = std::nullopt)
    -> std::expected<void, std::string>;
auto shutdown() -> void;
auto flush() -> void;
auto set_level(std::string_view level) -> std::expected<void, std::string>;

}  // namespace utils::logging

// Logger类 - 使用构造函数捕获source_location
class Logger {
 public:
  Logger(std::source_location loc = std::source_location::current());

  // 格式化日志函数
  template <typename... Args>
  inline auto trace(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto debug(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto info(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto warn(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto error(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
    spdlog::default_logger()->log(
        spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
        spdlog::level::err, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto critical(spdlog::format_string_t<Args...> fmt, Args&&... args) const -> void {
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
