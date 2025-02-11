#pragma once

#include <string>
#include <nlohmann/json.hpp>

// 处理状态枚举
enum class ProcessingStatus {
    Pending,    // 等待处理
    Processing, // 处理中
    Completed,  // 处理完成
    Failed      // 处理失败
};

// 处理进度信息
struct ProcessingProgress {
    ProcessingStatus status{ProcessingStatus::Pending};
    int total_files{0};
    int processed_files{0};
    std::string current_file;
    std::string error_message;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProcessingProgress,
        status, total_files, processed_files, current_file, error_message)
};

// JSON序列化支持
NLOHMANN_JSON_SERIALIZE_ENUM(ProcessingStatus, {
    {ProcessingStatus::Pending, "pending"},
    {ProcessingStatus::Processing, "processing"},
    {ProcessingStatus::Completed, "completed"},
    {ProcessingStatus::Failed, "failed"}
}) 