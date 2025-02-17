#pragma once

#include "media/db/models.hpp"
#include "media/repositories/screenshot_repository.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <utility>
#include <nlohmann/json.hpp>
#include <optional>

// 截图服务类 - 处理业务逻辑
class ScreenshotService {
public:
    // 获取单例实例
    static ScreenshotService& get_instance();

    // 创建新的截图记录
    Screenshot create_screenshot(const std::string& filepath);

    // 从文件创建截图对象（不保存到数据库）
    Screenshot create_from_file(const std::filesystem::path& filepath);

    // 获取截图列表
    std::vector<Screenshot> get_screenshots(bool include_deleted = false);

    // 获取分页后的截图列表
    std::pair<std::vector<Screenshot>, bool> get_screenshots_paginated(
        const std::string& folder_id,
        const std::string& relative_path,
        int64_t last_id, 
        int limit
    );

    // 获取月份统计信息
    std::vector<MonthStats> get_month_statistics();
    
    // 获取指定月份的照片
    std::pair<std::vector<Screenshot>, bool> get_screenshots_by_month(
        int year, 
        int month, 
        int64_t last_id, 
        int limit
    );

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

    // 获取相册照片
    std::pair<std::vector<Screenshot>, bool> get_screenshots_by_album(
        int64_t album_id, 
        int64_t last_id, 
        int limit
    );

    // 获取文件夹树结构
    std::vector<FolderTreeNode> get_folder_tree();

private:
    ScreenshotService() = default;
    ~ScreenshotService() = default;

    // 禁止拷贝和赋值
    ScreenshotService(const ScreenshotService&) = delete;
    ScreenshotService& operator=(const ScreenshotService&) = delete;

    // 从文件中读取图片信息
    void read_image_info(const std::string& filepath, Screenshot& screenshot);

    // 从文件名解析照片时间
    std::optional<int64_t> parse_photo_time_from_filename(const std::string& filename);

    // 构建文件夹树
    void build_folder_tree(FolderTreeNode& node, const std::string& path);

    // 计算相对路径
    std::string calculate_relative_path(const std::string& filepath, const std::string& base_path);

    // 数据访问层
    ScreenshotRepository& repository_ = ScreenshotRepository::get_instance();
}; 