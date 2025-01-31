#pragma once

#include "../db/models.hpp"
#include <string>
#include <vector>
#include <filesystem>

// 照片服务类
class PhotoService {
public:
    // 获取单例实例
    static PhotoService& get_instance();

    // 创建新的截图记录
    Screenshot create_screenshot(const std::string& filepath);

    // 获取截图列表
    std::vector<Screenshot> get_screenshots(bool include_deleted = false);

    // 获取单个截图
    Screenshot get_screenshot(int64_t id);

    // 更新截图信息
    bool update_screenshot(Screenshot& screenshot);

    // 删除截图
    bool delete_screenshot(int64_t id);

    // 获取截图文件的完整路径
    std::filesystem::path get_screenshot_path(const Screenshot& screenshot) const;

private:
    PhotoService() = default;
    ~PhotoService() = default;

    // 禁止拷贝和赋值
    PhotoService(const PhotoService&) = delete;
    PhotoService& operator=(const PhotoService&) = delete;

    // 从文件中读取图片信息
    void read_image_info(const std::string& filepath, Screenshot& screenshot);
}; 