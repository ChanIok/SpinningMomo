#pragma once

#include <filesystem>
#include <string>
#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include "thumbnail_batch_processor.hpp"
#include "media/repositories/screenshot_repository.hpp"
#include "media/db/models.hpp"

// 初始化服务类 - 负责系统初始化和数据同步
class InitializationService {
public:
    static InitializationService& get_instance();
    
    // 总体初始化流程
    void initialize();
    
    // 获取截图目录
    const std::filesystem::path& get_screenshot_directory() const {
        return screenshot_directory_;
    }

private:
    InitializationService() {
        // 在构造函数中设置默认路径
        screenshot_directory_ = LR"(D:\工程\2024\游戏\InfinityNikki\NikkiPhotos_HighQuality)";
    }
    ~InitializationService() = default;
    
    // 禁止拷贝和赋值
    InitializationService(const InitializationService&) = delete;
    InitializationService& operator=(const InitializationService&) = delete;
    
    // 初始化步骤
    void ensure_directories();
    void sync_screenshots();
    void generate_missing_thumbnails();
    void cleanup_invalid_data();
    
    // 成员变量
    std::filesystem::path screenshot_directory_;
    ScreenshotRepository& screenshot_repository_ = ScreenshotRepository::get_instance();
}; 