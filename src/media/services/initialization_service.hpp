#pragma once

#include <filesystem>
#include <string>
#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include "thumbnail_batch_processor.hpp"
#include "media/db/models.hpp"

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
        screenshot_directory_ = LR"(D:\Program Files\InfinityNikki Launcher\InfinityNikki\X6Game\ScreenShot)";
    }
    ~InitializationService() = default;
    
    // 禁止拷贝和赋值
    InitializationService(const InitializationService&) = delete;
    InitializationService& operator=(const InitializationService&) = delete;
    
    // 确保目录存在
    void ensure_directories();
    
    // 扫描目录并同步数据库
    void sync_screenshots();
    
    // 生成缺失的缩略图
    void generate_missing_thumbnails();
    
    // 清理无效数据
    void cleanup_invalid_data();
    
    std::filesystem::path screenshot_directory_;
}; 