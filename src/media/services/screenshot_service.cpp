#include "screenshot_service.hpp"
#include "thumbnail_service.hpp"
#include "media/utils/string_utils.hpp"
#include "media/utils/time_utils.hpp"
#include "core/image_processor.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include "core/win_config.hpp"

ScreenshotService& ScreenshotService::get_instance() {
    static ScreenshotService instance;
    return instance;
}

Screenshot ScreenshotService::create_screenshot(const std::string& filepath) {
    // 检查文件是否存在
    if (!std::filesystem::exists(filepath)) {
        throw std::runtime_error("文件不存在: " + filepath);
    }

    // 创建截图对象
    auto screenshot = create_from_file(std::filesystem::path(filepath));
    
    // 保存到数据库
    if (!repository_.save(screenshot)) {
        throw std::runtime_error("保存截图记录失败");
    }
    
    // 生成缩略图
    if (!ThumbnailService::get_instance().generate_thumbnail(screenshot)) {
        spdlog::warn("生成缩略图失败: {}", filepath);
    }
    
    return screenshot;
}

std::optional<int64_t> ScreenshotService::parse_photo_time_from_filename(const std::string& filename) {
    // 分割文件名 2024_12_05_23_50_29_6626579.jpeg
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = filename.find('_');
    
    while (end != std::string::npos) {
        parts.push_back(filename.substr(start, end - start));
        start = end + 1;
        end = filename.find('_', start);
    }
    
    // 如果格式不正确，返回空
    if (parts.size() < 6) {
        return std::nullopt;
    }
    
    try {
        // 解析时间组件
        struct tm timeinfo = {};
        timeinfo.tm_year = std::stoi(parts[0]) - 1900;  // 年份需要减去1900
        timeinfo.tm_mon = std::stoi(parts[1]) - 1;      // 月份从0开始
        timeinfo.tm_mday = std::stoi(parts[2]);
        timeinfo.tm_hour = std::stoi(parts[3]);
        timeinfo.tm_min = std::stoi(parts[4]);
        timeinfo.tm_sec = std::stoi(parts[5]);
        
        // 转换为时间戳
        return std::mktime(&timeinfo);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

Screenshot ScreenshotService::create_from_file(const std::filesystem::path& filepath) {
    Screenshot screenshot;
    // 确保使用UTF-8编码存储路径
    screenshot.filepath = wide_to_utf8(filepath.wstring());
    screenshot.filename = wide_to_utf8(filepath.filename().wstring());
    
    // 获取文件创建时间
    HANDLE hFile = CreateFileW(
        filepath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file: " + screenshot.filepath);
    }
    
    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (!GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(hFile);
        throw std::runtime_error("Failed to get file times: " + screenshot.filepath);
    }
    CloseHandle(hFile);
    
    // 转换FILETIME到Unix时间戳
    ULARGE_INTEGER uli;
    uli.LowPart = creationTime.dwLowDateTime;
    uli.HighPart = creationTime.dwHighDateTime;
    // 从1601年到1970年的时间差（单位：100纳秒）
    const uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    // 转换为Unix时间戳（秒）
    uint64_t timestamp = (uli.QuadPart - EPOCH_DIFFERENCE) / 10000000ULL;
    
    screenshot.created_at = static_cast<int64_t>(timestamp);
    screenshot.updated_at = TimeUtils::now();
    screenshot.file_size = std::filesystem::file_size(filepath);
    
    // 从文件名解析照片时间
    screenshot.photo_time = parse_photo_time_from_filename(screenshot.filename);
    
    // 使用WIC读取图片信息
    auto source = ImageProcessor::LoadFromFile(filepath);
    if (!source) {
        throw std::runtime_error("Failed to load image: " + screenshot.filepath);
    }
    
    // 获取图片尺寸
    ImageProcessor::GetImageDimensions(source.Get(), screenshot.width, screenshot.height);
    
    // 设置其他字段的默认值
    screenshot.metadata = "{}";  // 空的JSON对象
    screenshot.thumbnail_generated = false;
    
    return screenshot;
}

std::vector<Screenshot> ScreenshotService::get_screenshots(bool include_deleted) {
    return repository_.find_all(include_deleted);
}

Screenshot ScreenshotService::get_screenshot(int64_t id) {
    Screenshot screenshot = repository_.find_by_id(id);
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
    
    return repository_.save(screenshot);
}

bool ScreenshotService::delete_screenshot(int64_t id) {
    return repository_.remove(id);
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
    return repository_.find_paginated(last_id, limit);
}

std::vector<Screenshot> ScreenshotService::get_screenshots_by_directory(const std::wstring& directory) {
    return repository_.find_by_directory(directory);
}

std::pair<std::vector<char>, std::string> ScreenshotService::read_raw_image(const Screenshot& screenshot) {
    // 将UTF-8路径转换为宽字符
    std::wstring wpath = utf8_to_wide(screenshot.filepath);
    if (!std::filesystem::exists(std::filesystem::path(wpath))) {
        throw std::runtime_error("Image file not found");
    }
    
    // 使用宽字符路径打开文件
    std::basic_ifstream<char> file(wpath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open image file");
    }
    
    // 读取文件内容
    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    
    // 获取文件扩展名并设置对应的Content-Type
    auto ext = std::filesystem::path(wpath).extension().wstring();
    std::string ext_utf8 = wide_to_utf8(ext);
    std::transform(ext_utf8.begin(), ext_utf8.end(), ext_utf8.begin(), ::tolower);
    
    std::string content_type;
    if (ext_utf8 == ".jpg" || ext_utf8 == ".jpeg") {
        content_type = "image/jpeg";
    } else if (ext_utf8 == ".png") {
        content_type = "image/png";
    } else {
        content_type = "application/octet-stream";
    }
    
    return {buffer, content_type};
}

nlohmann::json ScreenshotService::get_screenshot_with_thumbnail(const Screenshot& screenshot) {
    nlohmann::json screenshot_json = screenshot;
    
    // 添加缩略图URL
    if (ThumbnailService::get_instance().thumbnail_exists(screenshot)) {
        screenshot_json["thumbnailPath"] = "/api/screenshots/" + 
            std::to_string(screenshot.id) + "/thumbnail";
    }
    
    return screenshot_json;
}

std::vector<MonthStats> ScreenshotService::get_month_statistics() {
    return repository_.get_month_statistics();
}

std::pair<std::vector<Screenshot>, bool> ScreenshotService::get_screenshots_by_month(int year, int month, int64_t last_id, int limit) {
    return repository_.find_by_month(year, month, last_id, limit);
}

std::pair<std::vector<Screenshot>, bool> ScreenshotService::get_screenshots_by_album(int64_t album_id, int64_t last_id, int limit) {
    return repository_.find_by_album(album_id, last_id, limit);
} 