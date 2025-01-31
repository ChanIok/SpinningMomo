#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/msvc_sink.h>
#include <filesystem>
#include "core/win_config.hpp"

class Logger {
public:
    static Logger& get_instance() {
        static Logger instance;
        return instance;
    }

    void init() {
        try {
            auto log_dir = get_log_directory();
            std::filesystem::create_directories(log_dir);
            auto log_file = log_dir / "spinning_momo.log";

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.string(), true);
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

            const char* pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";
            file_sink->set_pattern(pattern);
            console_sink->set_pattern(pattern);
            msvc_sink->set_pattern(pattern);

            std::vector<spdlog::sink_ptr> sinks {file_sink, console_sink, msvc_sink};
            auto logger = std::make_shared<spdlog::logger>("spinning_momo", sinks.begin(), sinks.end());

#ifdef _DEBUG
            logger->set_level(spdlog::level::debug);
#else
            logger->set_level(spdlog::level::info);
#endif

            // 设置日志刷新策略为每条日志消息后立即刷新
            logger->flush_on(spdlog::level::trace);
            
            // 将此logger注册为默认logger
            spdlog::set_default_logger(logger);
            
            // 输出测试日志以验证初始化
            logger->info("Logger initialized at: {}", log_file.string());
            logger->flush();
        }
        catch (const std::exception& e) {
            OutputDebugStringA("Failed to initialize logger: ");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
        }
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::filesystem::path get_log_directory() {
        return std::filesystem::current_path() / "logs";
    }
}; 
