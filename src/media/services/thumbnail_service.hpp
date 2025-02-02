#pragma once

#include "win_config.hpp"
#include <string>
#include <filesystem>
#include "media/db/models.hpp"

class ThumbnailService {
public:
    static ThumbnailService& get_instance();
    
    // 生成缩略图
    bool generate_thumbnail(const Screenshot& screenshot);
    
    // 批量生成缩略图
    bool generate_thumbnails(const std::vector<Screenshot>& screenshots);
    
    // 获取缩略图路径
    std::filesystem::path get_thumbnail_path(const Screenshot& screenshot) const;
    
    // 检查缩略图是否存在
    bool thumbnail_exists(const Screenshot& screenshot) const;
    
    // 确保缩略图目录存在
    void ensure_thumbnail_dir() const;

private:
    ThumbnailService() {
        // 初始化 COM
        if (FAILED(CoInitialize(nullptr))) {
            throw std::runtime_error("Failed to initialize COM");
        }
        
        // 设置缩略图目录为可执行文件所在目录下的 data/thumbnails
        auto exe_path = std::filesystem::current_path();
        thumbnail_dir_ = exe_path / "data" / "thumbnails";
    }
    
    ~ThumbnailService() {
        // 清理 COM
        CoUninitialize();
    }
    
    ThumbnailService(const ThumbnailService&) = delete;
    ThumbnailService& operator=(const ThumbnailService&) = delete;
    
    // 缩略图目录
    std::filesystem::path thumbnail_dir_;
}; 