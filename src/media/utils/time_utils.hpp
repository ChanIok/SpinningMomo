#pragma once

#include <chrono>
#include <filesystem>

/**
 * @brief 时间工具类，提供基本的时间转换功能
 */
class TimeUtils {
public:
    /**
     * @brief 获取当前的Unix时间戳（秒）
     * @return int64_t Unix时间戳
     */
    static int64_t now() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief 将文件时间转换为Unix时间戳
     * @param file_time 文件时间
     * @return int64_t Unix时间戳
     */
    static int64_t file_time_to_timestamp(const std::filesystem::file_time_type& file_time) {
        // 将文件时间转换为系统时间
        auto sys_time = std::chrono::system_clock::now() + (file_time - std::filesystem::file_time_type::clock::now());
        return std::chrono::duration_cast<std::chrono::seconds>(
            sys_time.time_since_epoch()
        ).count();
    }
}; 
