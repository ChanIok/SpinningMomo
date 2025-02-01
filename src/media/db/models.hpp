#pragma once

#include <string>
#include <vector>
#include <optional>
#include <ctime>
#include <nlohmann/json.hpp>
#include <filesystem>

// 截图模型
class Screenshot {
public:
    int64_t id = 0;
    std::string filename;
    std::string filepath;
    std::time_t created_at = 0;
    int width = 0;
    int height = 0;
    int64_t file_size = 0;
    std::string metadata;
    std::optional<std::time_t> deleted_at;
    std::time_t updated_at = 0;

    // 基本的CRUD操作
    static Screenshot find_by_id(int64_t id);
    static std::vector<Screenshot> find_all(bool include_deleted = false);
    // 基于目录的操作
    static std::vector<Screenshot> find_by_directory(const std::wstring& dir_path, 
                                                   int64_t last_id = 0,
                                                   int limit = 20);
    static bool has_more(const std::wstring& dir_path, int64_t last_id);
    static Screenshot from_file(const std::filesystem::path& file_path);
    bool save();
    bool remove();

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
            {"deleted_at", deleted_at.has_value() ? deleted_at.value() : 0}
        };
    }

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
        return screenshot;
    }
};

// 相册模型
class Album {
public:
    int64_t id = 0;
    std::string name;
    std::string description;
    std::optional<int64_t> cover_screenshot_id;
    std::time_t created_at = 0;
    std::time_t updated_at = 0;
    std::optional<std::time_t> deleted_at;

    // 基本的CRUD操作
    static Album find_by_id(int64_t id);
    static std::vector<Album> find_all(bool include_deleted = false);
    bool save();
    bool remove();

    // 相册特有操作
    bool add_screenshot(int64_t screenshot_id, int position);
    bool remove_screenshot(int64_t screenshot_id);
    std::vector<Screenshot> get_screenshots() const;

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
}; 