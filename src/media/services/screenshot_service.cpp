#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

ScreenshotService& ScreenshotService::get_instance() {
    static ScreenshotService instance;
    return instance;
}

Screenshot ScreenshotService::create_screenshot(const std::string& filepath) {
    // 检查文件是否存在
    if (!std::filesystem::exists(filepath)) {
        throw std::runtime_error("文件不存在: " + filepath);
    }

    Screenshot screenshot;
    screenshot.filepath = filepath;
    screenshot.filename = std::filesystem::path(filepath).filename().string();
    
    // 生成唯一ID
    screenshot.id = Screenshot::generate_id(filepath, screenshot.filename);
    
    // 读取图片信息
    read_image_info(filepath, screenshot);
    
    // 保存到数据库
    if (!screenshot.save()) {
        throw std::runtime_error("保存截图记录失败");
    }
    
    // 生成缩略图
    if (!ThumbnailService::get_instance().generate_thumbnail(screenshot)) {
        spdlog::warn("生成缩略图失败: {}", filepath);
    }
    
    return screenshot;
}

std::vector<Screenshot> ScreenshotService::get_screenshots(bool include_deleted) {
    return Screenshot::find_all(include_deleted);
}

Screenshot ScreenshotService::get_screenshot(int64_t id) {
    Screenshot screenshot = Screenshot::find_by_id(id);
    if (screenshot.id == 0) {
        throw std::runtime_error("截图不存在");
    }
    return screenshot;
}

bool ScreenshotService::update_screenshot(Screenshot& screenshot) {
    if (screenshot.id == 0) {
        return false;
    }
    
    // 检查文件是否存在
    if (!std::filesystem::exists(screenshot.filepath)) {
        return false;
    }
    
    return screenshot.save();
}

bool ScreenshotService::delete_screenshot(int64_t id) {
    Screenshot screenshot = Screenshot::find_by_id(id);
    if (screenshot.id == 0) {
        return false;
    }
    return screenshot.remove();
}

std::filesystem::path ScreenshotService::get_screenshot_path(const Screenshot& screenshot) const {
    return std::filesystem::path(screenshot.filepath);
}

void ScreenshotService::read_image_info(const std::string& filepath, Screenshot& screenshot) {
    // 获取文件大小
    screenshot.file_size = std::filesystem::file_size(filepath);
    
    // TODO: 使用图片处理库读取图片尺寸
    // 这里暂时使用默认值
    screenshot.width = 1920;
    screenshot.height = 1080;
} 