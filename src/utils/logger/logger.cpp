module;

module Utils.Logger;

import std;
import Utils.Path;
import Vendor.BuildConfig;
import <spdlog/sinks/msvc_sink.h>;
import <spdlog/sinks/rotating_file_sink.h>;
import <spdlog/spdlog.h>;

namespace Utils::Logging::Detail {

auto default_level() -> spdlog::level::level_enum {
  return Vendor::BuildConfig::is_debug_build() ? spdlog::level::trace : spdlog::level::info;
}

auto normalize_level_string(std::string_view level) -> std::string {
  std::string normalized;
  normalized.reserve(level.size());

  for (const auto ch : level) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
  }

  return normalized;
}

auto parse_level(std::string_view level) -> std::expected<spdlog::level::level_enum, std::string> {
  const auto normalized = normalize_level_string(level);

  if (normalized.empty()) {
    return std::unexpected("Logger level is empty");
  }

  if (normalized == "TRACE") {
    return spdlog::level::trace;
  }
  if (normalized == "DEBUG") {
    return spdlog::level::debug;
  }
  if (normalized == "INFO") {
    return spdlog::level::info;
  }
  if (normalized == "WARN" || normalized == "WARNING") {
    return spdlog::level::warn;
  }
  if (normalized == "ERROR" || normalized == "ERR") {
    return spdlog::level::err;
  }
  if (normalized == "CRITICAL") {
    return spdlog::level::critical;
  }
  if (normalized == "OFF") {
    return spdlog::level::off;
  }

  return std::unexpected("Unsupported logger level: " + std::string(level));
}

}  // namespace Utils::Logging::Detail

// 构造函数实现
Logger::Logger(std::source_location loc) : loc_(std::move(loc)) {}

// 简单字符串日志函数实现
auto Logger::trace(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::trace, msg);
}

auto Logger::debug(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::debug, msg);
}

auto Logger::info(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::info, msg);
}

auto Logger::warn(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::warn, msg);
}

auto Logger::error(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::err, msg);
}

auto Logger::critical(std::string_view msg) const -> void {
  spdlog::default_logger()->log(
      spdlog::source_loc{loc_.file_name(), static_cast<int>(loc_.line()), loc_.function_name()},
      spdlog::level::critical, msg);
}

// 日志管理函数实现
namespace Utils::Logging {

auto initialize(const std::optional<std::string>& configured_level)
    -> std::expected<void, std::string> {
  try {
    auto logs_dir_result = Utils::Path::GetAppDataSubdirectory("logs");
    if (!logs_dir_result) {
      return std::unexpected("Failed to get log directory: " + logs_dir_result.error());
    }

    auto log_file_path = logs_dir_result.value() / "app.log";

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path.string(),
                                                                           5 * 1024 * 1024, 3));

    auto logger = std::make_shared<spdlog::logger>("spinning_momo", sinks.begin(), sinks.end());

    logger->set_pattern(Vendor::BuildConfig::is_debug_build()
                            ? "%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%g:%#] %v"
                            : "%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%s:%#] %v");

    auto resolved_level = Detail::default_level();
    std::optional<std::string> level_warning;
    if (configured_level.has_value() && !configured_level->empty()) {
      auto parse_result = Detail::parse_level(configured_level.value());
      if (parse_result) {
        resolved_level = parse_result.value();
      } else {
        level_warning = parse_result.error() + ", fallback to default level";
      }
    }

    logger->set_level(resolved_level);
    logger->flush_on(spdlog::level::trace);
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);

    if (level_warning.has_value()) {
      logger->warn("{}", level_warning.value());
    }

    return {};
  } catch (const spdlog::spdlog_ex& ex) {
    return std::unexpected(std::string("Log initialization failed: ") + ex.what());
  }
}

auto shutdown() -> void {
  auto logger = spdlog::default_logger();
  if (logger) {
    logger->flush();
  }
  spdlog::shutdown();
}

auto flush() -> void {
  auto logger = spdlog::default_logger();
  if (logger) {
    logger->flush();
  }
}

auto set_level(std::string_view level) -> std::expected<void, std::string> {
  auto logger = spdlog::default_logger();
  if (!logger) {
    return std::unexpected("Logger is not initialized");
  }

  auto parse_result = Detail::parse_level(level);
  if (!parse_result) {
    return std::unexpected(parse_result.error());
  }

  logger->set_level(parse_result.value());
  logger->flush();
  return {};
}

}  // namespace Utils::Logging
