#pragma once

#include "win_config.hpp"
#include <string>
#include <vector>
#include <optional>
#include <ctime>
#include <nlohmann/json.hpp>
#include <filesystem>

// 截图数据结构
struct Screenshot {
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
    std::optional<int64_t> photo_time;     // 照片拍摄时间（Unix时间戳）
    std::string folder_id;                 // 关联的监控文件夹ID
    std::string relative_path;             // 相对于监控文件夹的路径

    bool is_valid() const { return !filename.empty() && !filepath.empty(); }
};

// 文件夹树节点结构
struct FolderTreeNode {
    std::string name;                      // 文件夹名称
    std::string full_path;                 // 完整路径
    std::string folder_id;                 // 根文件夹ID
    int photo_count;                       // 该目录下的照片数量
    std::vector<FolderTreeNode> children;  // 子文件夹

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FolderTreeNode,
        name, full_path, folder_id, photo_count, children)
};

// 相册数据结构
struct Album {
    int64_t id = 0;                        // 唯一标识符
    std::string name;                      // 相册名称
    std::string description;               // 相册描述
    std::optional<int64_t> cover_screenshot_id; // 封面截图ID
    std::time_t created_at = 0;            // 创建时间
    std::time_t updated_at = 0;            // 更新时间
    std::optional<std::time_t> deleted_at; // 删除时间

    bool is_valid() const { return !name.empty(); }
};

// 月份统计信息
struct MonthStats {
    int year;                   // 年份
    int month;                  // 月份
    int count;                  // 照片数量
    int64_t first_screenshot_id;// 第一张照片ID
};

// JSON 序列化支持
inline void to_json(nlohmann::json& j, const Screenshot& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"filename", s.filename},
        {"filepath", s.filepath},
        {"width", s.width},
        {"height", s.height},
        {"file_size", s.file_size},
        {"metadata", s.metadata},
        {"created_at", s.created_at},
        {"updated_at", s.updated_at},
        {"deleted_at", s.deleted_at.has_value() ? s.deleted_at.value() : 0},
        {"thumbnail_generated", s.thumbnail_generated},
        {"photo_time", s.photo_time.has_value() ? s.photo_time.value() : 0},
        {"folder_id", s.folder_id},
        {"relative_path", s.relative_path}
    };
}

inline void from_json(const nlohmann::json& j, Screenshot& s) {
    j.at("id").get_to(s.id);
    j.at("filename").get_to(s.filename);
    j.at("filepath").get_to(s.filepath);
    s.width = j.value("width", 0);
    s.height = j.value("height", 0);
    s.file_size = j.value("file_size", 0);
    s.metadata = j.value("metadata", "");
    s.created_at = j.value("created_at", 0);
    s.updated_at = j.value("updated_at", 0);
    s.deleted_at = j.value("deleted_at", 0);
    s.thumbnail_generated = j.value("thumbnail_generated", false);
    s.photo_time = j.value("photo_time", 0);
    s.folder_id = j.value("folder_id", "");
    s.relative_path = j.value("relative_path", "");
}

inline void to_json(nlohmann::json& j, const Album& a) {
    j = nlohmann::json{
        {"id", a.id},
        {"name", a.name},
        {"description", a.description},
        {"cover_screenshot_id", a.cover_screenshot_id.has_value() ? a.cover_screenshot_id.value() : 0},
        {"created_at", a.created_at},
        {"updated_at", a.updated_at},
        {"deleted_at", a.deleted_at.has_value() ? a.deleted_at.value() : 0}
    };
}

inline void from_json(const nlohmann::json& j, Album& a) {
    j.at("id").get_to(a.id);
    j.at("name").get_to(a.name);
    a.description = j.value("description", "");
    a.cover_screenshot_id = j.value("cover_screenshot_id", 0);
    a.created_at = j.value("created_at", 0);
    a.updated_at = j.value("updated_at", 0);
    a.deleted_at = j.value("deleted_at", 0);
}

inline void to_json(nlohmann::json& j, const MonthStats& m) {
    j = nlohmann::json{
        {"year", m.year},
        {"month", m.month},
        {"count", m.count},
        {"first_screenshot_id", m.first_screenshot_id}
    };
}

inline void from_json(const nlohmann::json& j, MonthStats& m) {
    j.at("year").get_to(m.year);
    j.at("month").get_to(m.month);
    j.at("count").get_to(m.count);
    j.at("first_screenshot_id").get_to(m.first_screenshot_id);
} 