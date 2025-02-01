#include "thumbnail_service.hpp"
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include "media/db/database.hpp"
#include "core/image_processor.hpp"
#include "thumbnail_batch_processor.hpp"

ThumbnailService& ThumbnailService::get_instance() {
    static ThumbnailService instance;
    return instance;
}

bool ThumbnailService::generate_thumbnail(const Screenshot& screenshot) {
    try {
        spdlog::info("Adding thumbnail generation task for: {}", screenshot.filename);
        ensure_thumbnail_dir();
        
        // 检查源文件是否存在
        if (!std::filesystem::exists(screenshot.filepath)) {
            spdlog::error("Source file does not exist: {}", screenshot.filepath);
            return false;
        }
        
        // 添加到批处理队列
        ThumbnailBatchProcessor::get_instance().add_task(screenshot);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to generate thumbnail: {}", e.what());
        return false;
    }
}

std::filesystem::path ThumbnailService::get_thumbnail_path(const Screenshot& screenshot) const {
    return thumbnail_dir_ / ("thumb_" + std::to_string(screenshot.id) + ".jpg");
}

bool ThumbnailService::thumbnail_exists(const Screenshot& screenshot) const {
    return std::filesystem::exists(get_thumbnail_path(screenshot));
}

void ThumbnailService::ensure_thumbnail_dir() const {
    std::filesystem::create_directories(thumbnail_dir_);
} 