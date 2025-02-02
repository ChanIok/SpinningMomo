#pragma once

#include "win_config.hpp"
#include <string>
#include <vector>
#include <optional>
#include <ctime>
#include <nlohmann/json.hpp>
#include <filesystem>

// 截图模型 - 纯数据结构，不包含数据库操作
class Screenshot {
public:
    int64_t id = 0;                        // 唯一标识符
    std::string filename;                   // 文件名
    std::string filepath;                   // 文件路径
    int64_t created_at = 0;                // 创建时间（Unix时间戳）
    UINT width = 0;                        // 图片宽度
    UINT height = 0;                       // 图片高度
    int64_t file_size = 0;                 // 文件大小
    std::string metadata;                  // 元数据（JSON格式）
    std::optional<int64_t> deleted_at;     // 删除时间（Unix时间戳）
    int64_t updated_at = 0;                // 更新时间（Unix时间戳）
    bool thumbnail_generated = false;       // 缩略图是否已生成

    // 序列化方法
    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"filename", filename},
            {"filepath", filepath},
            {"width", width},
            {"height", height},
            {"file_size", file_size},
            {"metadata", metadata},
            {"created_at", created_at},
            {"updated_at", updated_at},
            {"deleted_at", deleted_at.has_value() ? deleted_at.value() : 0},
            {"thumbnail_generated", thumbnail_generated}
        };
    }

    // 反序列化方法
    static Screenshot from_json(const nlohmann::json& j) {
        Screenshot screenshot;
        screenshot.id = j["id"].get<int64_t>();
        screenshot.filename = j["filename"].get<std::string>();
        screenshot.filepath = j["filepath"].get<std::string>();
        screenshot.width = j.value("width", 0);
        screenshot.height = j.value("height", 0);
        screenshot.file_size = j.value("file_size", 0);
        screenshot.metadata = j.value("metadata", "");
        screenshot.created_at = j.value("created_at", 0);
        screenshot.updated_at = j.value("updated_at", 0);
        screenshot.deleted_at = j.value("deleted_at", 0);
        screenshot.thumbnail_generated = j.value("thumbnail_generated", false);
        return screenshot;
    }

    // 基本验证方法
    bool is_valid() const {
        return !filename.empty() && !filepath.empty();
    }
};

// 相册模型 - 纯数据结构，不包含数据库操作
class Album {
public:
    int64_t id = 0;                        // 唯一标识符
    std::string name;                      // 相册名称
    std::string description;               // 相册描述
    std::optional<int64_t> cover_screenshot_id; // 封面截图ID
    std::time_t created_at = 0;            // 创建时间
    std::time_t updated_at = 0;            // 更新时间
    std::optional<std::time_t> deleted_at; // 删除时间

    // 序列化方法
    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"name", name},
            {"description", description},
            {"cover_screenshot_id", cover_screenshot_id.has_value() ? cover_screenshot_id.value() : 0},
            {"created_at", created_at},
            {"updated_at", updated_at},
            {"deleted_at", deleted_at.has_value() ? deleted_at.value() : 0}
        };
    }

    // 反序列化方法
    static Album from_json(const nlohmann::json& j) {
        Album album;
        album.id = j["id"].get<int64_t>();
        album.name = j["name"].get<std::string>();
        album.description = j.value("description", "");
        album.cover_screenshot_id = j.value("cover_screenshot_id", 0);
        album.created_at = j.value("created_at", 0);
        album.updated_at = j.value("updated_at", 0);
        album.deleted_at = j.value("deleted_at", 0);
        return album;
    }

    // 基本验证方法
    bool is_valid() const {
        return !name.empty();
    }
}; 