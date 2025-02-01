#pragma once

#include "media/db/models.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <utility>
#include <nlohmann/json.hpp>

// 截图服务类
class ScreenshotService {
public:
    // 获取单例实例
    static ScreenshotService& get_instance();

    // 创建新的截图记录
    Screenshot create_screenshot(const std::string& filepath);

    // 获取截图列表
    std::vector<Screenshot> get_screenshots(bool include_deleted = false);

    // 获取分页后的截图列表
    std::pair<std::vector<Screenshot>, bool> get_screenshots_paginated(int64_t last_id, int limit);

    // 获取指定目录下的截图
    std::vector<Screenshot> get_screenshots_by_directory(const std::wstring& directory);

    // 获取单个截图
    Screenshot get_screenshot(int64_t id);

    // 更新截图信息
    bool update_screenshot(Screenshot& screenshot);

    // 删除截图
    bool delete_screenshot(int64_t id);

    // 获取截图文件的完整路径
    std::filesystem::path get_screenshot_path(const Screenshot& screenshot) const;

    // 读取原始图片文件内容
    std::pair<std::vector<char>, std::string> read_raw_image(const Screenshot& screenshot);

    // 获取截图的完整信息（包含缩略图URL）
    nlohmann::json get_screenshot_with_thumbnail(const Screenshot& screenshot);

private:
    ScreenshotService() = default;
    ~ScreenshotService() = default;

    // 禁止拷贝和赋值
    ScreenshotService(const ScreenshotService&) = delete;
    ScreenshotService& operator=(const ScreenshotService&) = delete;

    // 从文件中读取图片信息
    void read_image_info(const std::string& filepath, Screenshot& screenshot);
}; 