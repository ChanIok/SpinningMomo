#pragma once

#include <filesystem>
#include <string>
#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include "thumbnail_batch_processor.hpp"
#include "folder_monitor/folder_monitor_service.hpp"
#include "media/repositories/screenshot_repository.hpp"
#include "media/db/models.hpp"

// 初始化服务类 - 负责系统初始化和数据同步
class InitializationService {
public:
    static InitializationService& get_instance();
    
    // 总体初始化流程
    void initialize();

private:
    InitializationService() = default;
    ~InitializationService() = default;
    
    // 禁止拷贝和赋值
    InitializationService(const InitializationService&) = delete;
    InitializationService& operator=(const InitializationService&) = delete;
    
    // 初始化步骤
    void ensure_directories();
    void init_folder_monitor();
    
    // 成员变量
    FolderMonitorService& folder_monitor_service_ = FolderMonitorService::get_instance();
}; 