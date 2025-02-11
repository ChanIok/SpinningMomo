#include "initialization_service.hpp"
#include "screenshot_service.hpp"
#include "media/utils/string_utils.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <vector>
#include <algorithm>
#include "media/db/database.hpp"

// 全局静态指针
static InitializationService* g_instance = nullptr;

// 获取单例实例
InitializationService& InitializationService::get_instance() {
    if (!g_instance) {
        g_instance = new InitializationService();
    }
    return *g_instance;
}

// 执行整体初始化流程
void InitializationService::initialize() {
    try {
        spdlog::info("Starting initialization process...");
        
        ensure_directories();
        init_folder_monitor();
        
        spdlog::info("Initialization completed successfully");
    } catch (const std::exception& e) {
        spdlog::error("Initialization failed: {}", e.what());
        throw;
    }
}

// 初始化文件夹监控服务
void InitializationService::init_folder_monitor() {
    try {
        spdlog::info("Initializing folder monitor service...");
        if (!folder_monitor_service_.init()) {
            throw std::runtime_error("Failed to initialize folder monitor service");
        }
        spdlog::info("Folder monitor service initialized successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize folder monitor service: {}", e.what());
        throw;
    }
}

// 确保必要的目录存在
void InitializationService::ensure_directories() {
    spdlog::info("Ensuring directories exist...");
    
    // 确保缩略图目录存在
    ThumbnailService::get_instance().ensure_thumbnail_dir();
}

// 同步文件系统中的截图到数据库
// void InitializationService::sync_screenshots() {
//     spdlog::info("Syncing screenshots with database...");
    
//     std::vector<Screenshot> existing_screenshots = screenshot_repository_.find_all(true);
//     spdlog::info("Found {} existing screenshots in database", existing_screenshots.size());
    
//     std::vector<Screenshot> new_screenshots;
//     int processed_count = 0;
    
//     // 扫描目录中的所有图片文件
//     for (const auto& entry : std::filesystem::directory_iterator(screenshot_directory_)) {
//         if (!entry.is_regular_file()) continue;
        
//         auto ext = entry.path().extension();
//         if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
        
//         processed_count++;
//         try {
//             // 从文件创建Screenshot对象
//             auto screenshot = ScreenshotService::get_instance().create_from_file(entry.path());
            
//             // 检查是否已存在
//             auto it = std::find_if(existing_screenshots.begin(), existing_screenshots.end(),
//                 [&screenshot](const Screenshot& s) { 
//                     return s.filepath == screenshot.filepath; 
//                 });
            
//             if (it == existing_screenshots.end()) {
//                 // 新文件，保存到数据库
//                 if (screenshot_repository_.save(screenshot)) {
//                     new_screenshots.push_back(screenshot);
//                     spdlog::info("Added new screenshot: {}", screenshot.filename);
//                 } else {
//                     spdlog::error("Failed to save screenshot to database: {}", screenshot.filename);
//                 }
//             }
//         } catch (const std::exception& e) {
//             spdlog::error("Error processing file {}: {}", entry.path().string(), e.what());
//         }
//     }
    
//     spdlog::info("Processed {} files, added {} new screenshots", processed_count, new_screenshots.size());
// }

// // 为缺失缩略图的截图生成缩略图
// void InitializationService::generate_missing_thumbnails() {
//     spdlog::info("Generating missing thumbnails...");
    
//     auto& thumbnail_service = ThumbnailService::get_instance();
//     auto screenshots = screenshot_repository_.find_all(false);
//     spdlog::info("Found {} screenshots in database for thumbnail generation", screenshots.size());
    
//     int generated_count = 0;
//     int failed_count = 0;
    
//     for (auto& screenshot : screenshots) {
//         if (!thumbnail_service.thumbnail_exists(screenshot)) {
//             try {
//                 if (thumbnail_service.generate_thumbnail(screenshot)) {
//                     if (screenshot_repository_.update_thumbnail_generated(screenshot.id, true)) {
//                         generated_count++;
//                         spdlog::debug("Generated thumbnail for: {}", screenshot.filename);
//                     } else {
//                         failed_count++;
//                         spdlog::error("Failed to update thumbnail_generated flag for: {}", screenshot.filename);
//                     }
//                 } else {
//                     failed_count++;
//                     spdlog::error("Failed to generate thumbnail for: {}", screenshot.filename);
//                 }
//             } catch (const std::exception& e) {
//                 failed_count++;
//                 spdlog::error("Failed to generate thumbnail for {}: {}", 
//                     screenshot.filename, e.what());
//             }
//         }
//     }
    
//     spdlog::info("Generated {} thumbnails, failed {}", generated_count, failed_count);
// }