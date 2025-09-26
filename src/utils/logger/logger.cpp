module;

module Utils.Logger;

import std;
import Vendor.BuildConfig;
import <spdlog/async.h>;
import <spdlog/sinks/msvc_sink.h>;
import <spdlog/sinks/rotating_file_sink.h>;
import <spdlog/spdlog.h>;

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

auto initialize() -> std::expected<void, std::string> {
  try {
    spdlog::init_thread_pool(8192, 1);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/infinity_momo.log",
                                                                           5 * 1024 * 1024, 3));

    auto async_logger = std::make_shared<spdlog::async_logger>(
        "infinity_momo", sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    async_logger->set_pattern(Vendor::BuildConfig::is_debug_build()
                                  ? "%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%g:%#] %v"
                                  : "%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%s:%#] %v");
    spdlog::flush_every(std::chrono::seconds(5));

    async_logger->set_level(Vendor::BuildConfig::is_debug_build() ? spdlog::level::trace
                                                                  : spdlog::level::info);

    async_logger->flush_on(spdlog::level::err);
    spdlog::register_logger(async_logger);
    spdlog::set_default_logger(async_logger);
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

}  // namespace Utils::Logging