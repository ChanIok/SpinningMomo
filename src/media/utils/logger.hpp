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
            auto log_dir = std::filesystem::current_path() / L"logs";
            std::filesystem::create_directories(log_dir);
            auto log_file = log_dir / L"spinning_momo.log";

            // 创建多个输出目标
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.wstring(), true);
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

            // 设置日志格式
            const char* pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";
            file_sink->set_pattern(pattern);
            console_sink->set_pattern(pattern);
            msvc_sink->set_pattern(pattern);

            // 创建并配置日志器
            std::vector<spdlog::sink_ptr> sinks {file_sink, console_sink, msvc_sink};
            auto logger = std::make_shared<spdlog::logger>("spinning_momo", sinks.begin(), sinks.end());

            // 设置日志级别
#ifdef _DEBUG
            logger->set_level(spdlog::level::debug);
#else
            logger->set_level(spdlog::level::info);
#endif

            // 设置立即刷新
            logger->flush_on(spdlog::level::trace);
            
            // 设置为默认日志器
            spdlog::set_default_logger(logger);
            
            // 输出初始化日志
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
}; 
