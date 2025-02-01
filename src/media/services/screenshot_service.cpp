#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <algorithm>

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

std::pair<std::vector<Screenshot>, bool> ScreenshotService::get_screenshots_paginated(int64_t last_id, int limit) {
    auto screenshots = get_screenshots(false);
    
    // 应用分页
    auto start = screenshots.begin();
    if (last_id > 0) {
        start = std::find_if(screenshots.begin(), screenshots.end(),
            [last_id](const Screenshot& s) { return s.id > last_id; });
    }
    
    auto end = start;
    std::advance(end, std::min<int>(limit, std::distance(start, screenshots.end())));
    
    bool has_more = end != screenshots.end();
    
    return {std::vector<Screenshot>(start, end), has_more};
}

std::vector<Screenshot> ScreenshotService::get_screenshots_by_directory(const std::wstring& directory) {
    return Screenshot::find_by_directory(directory);
}

std::pair<std::vector<char>, std::string> ScreenshotService::read_raw_image(const Screenshot& screenshot) {
    if (!std::filesystem::exists(screenshot.filepath)) {
        throw std::runtime_error("Image file not found");
    }
    
    // 读取图片文件
    std::ifstream file(screenshot.filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open image file");
    }
    
    // 读取文件内容
    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    
    // 获取文件扩展名并设置对应的Content-Type
    auto ext = std::filesystem::path(screenshot.filepath).extension().string();
    std::string content_type;
    if (ext == ".jpg" || ext == ".jpeg") {
        content_type = "image/jpeg";
    } else if (ext == ".png") {
        content_type = "image/png";
    } else {
        content_type = "application/octet-stream";
    }
    
    return {buffer, content_type};
}

nlohmann::json ScreenshotService::get_screenshot_with_thumbnail(const Screenshot& screenshot) {
    auto screenshot_json = screenshot.to_json();
    
    // 添加缩略图URL
    if (ThumbnailService::get_instance().thumbnail_exists(screenshot)) {
        screenshot_json["thumbnailPath"] = "/api/screenshots/" + 
            std::to_string(screenshot.id) + "/thumbnail";
    }
    
    return screenshot_json;
} 