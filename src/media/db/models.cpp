#include "models.hpp"
#include "database.hpp"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"
#include "core/image_processor.hpp"
#include "media/utils/time_utils.hpp"

// 数据库操作辅助函数
namespace {
    // 绑定Screenshot对象的通用字段到SQL语句
    void bind_screenshot_fields(sqlite3_stmt* stmt, const Screenshot& screenshot, bool include_id = false) {
        int idx = 1;
        if (include_id) {
            sqlite3_bind_int64(stmt, idx++, screenshot.id);
            }
        sqlite3_bind_text(stmt, idx++, screenshot.filename.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, idx++, screenshot.filepath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, idx++, screenshot.created_at);
        sqlite3_bind_int(stmt, idx++, screenshot.width);
        sqlite3_bind_int(stmt, idx++, screenshot.height);
        sqlite3_bind_int64(stmt, idx++, screenshot.file_size);
        sqlite3_bind_text(stmt, idx++, screenshot.metadata.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, idx++, screenshot.updated_at);
    }

    // 从SQL查询结果中读取Screenshot对象
    Screenshot read_screenshot_from_stmt(sqlite3_stmt* stmt) {
        Screenshot screenshot;
        int idx = 0;
        screenshot.id = sqlite3_column_int64(stmt, idx++);
        screenshot.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));
        screenshot.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));
        screenshot.created_at = sqlite3_column_int64(stmt, idx++);
        screenshot.width = sqlite3_column_int(stmt, idx++);
        screenshot.height = sqlite3_column_int(stmt, idx++);
        screenshot.file_size = sqlite3_column_int64(stmt, idx++);
        if (auto text = sqlite3_column_text(stmt, idx)) {
            screenshot.metadata = reinterpret_cast<const char*>(text);
        }
        idx++;
        if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
            screenshot.deleted_at = sqlite3_column_int64(stmt, idx);
        }
        idx++;
        screenshot.updated_at = sqlite3_column_int64(stmt, idx);
        return screenshot;
    }

    // 绑定Album对象的通用字段到SQL语句
    void bind_album_fields(sqlite3_stmt* stmt, const Album& album, bool include_id = false) {
        int idx = 1;
        if (include_id) {
            sqlite3_bind_int64(stmt, idx++, album.id);
        }
        sqlite3_bind_text(stmt, idx++, album.name.c_str(), -1, SQLITE_STATIC);
        if (!album.description.empty()) {
            sqlite3_bind_text(stmt, idx++, album.description.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, idx++);
        }
        if (album.cover_screenshot_id) {
            sqlite3_bind_int64(stmt, idx++, *album.cover_screenshot_id);
        } else {
            sqlite3_bind_null(stmt, idx++);
        }
    }

    // 从SQL查询结果中读取Album对象
    Album read_album_from_stmt(sqlite3_stmt* stmt) {
        Album album;
        int idx = 0;
        album.id = sqlite3_column_int64(stmt, idx++);
        album.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));
        if (auto text = sqlite3_column_text(stmt, idx)) {
            album.description = reinterpret_cast<const char*>(text);
        }
        idx++;
        if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
            album.cover_screenshot_id = sqlite3_column_int64(stmt, idx);
        }
        idx++;
        album.created_at = sqlite3_column_int64(stmt, idx++);
        album.updated_at = sqlite3_column_int64(stmt, idx++);
        if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
            album.deleted_at = sqlite3_column_int64(stmt, idx);
        }
        return album;
    }

    // 执行预处理语句并处理错误
    bool execute_statement(sqlite3_stmt* stmt, const std::string& operation) {
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error_msg = sqlite3_errmsg(Database::get_instance().get_handle());
            spdlog::error("执行{}失败: {}", operation, error_msg);
            return false;
        }
        return true;
    }

    // 准备SQL语句并进行错误处理
    sqlite3_stmt* prepare_statement(const char* sql) {
        sqlite3* db = Database::get_instance().get_handle();
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::string error_msg = sqlite3_errmsg(db);
            spdlog::error("SQL语句准备失败: {}", error_msg);
            throw DatabaseException("SQL语句准备失败");
        }
        return stmt;
    }
}

// 根据ID获取截图
Screenshot Screenshot::find_by_id(int64_t id) {
    const char* sql = R"(
        SELECT id, filename, filepath, created_at, width, height, 
               file_size, metadata, deleted_at, updated_at
        FROM screenshots WHERE id = ?
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, id);
    
    Screenshot screenshot;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        screenshot = read_screenshot_from_stmt(stmt);
    }
    
    sqlite3_finalize(stmt);
    return screenshot;
}

// 获取所有截图
std::vector<Screenshot> Screenshot::find_all(bool include_deleted) {
    std::string sql = R"(
        SELECT id, filename, filepath, created_at, width, height, 
               file_size, metadata, deleted_at, updated_at
        FROM screenshots
    )";
    if (!include_deleted) {
        sql += " WHERE deleted_at IS NULL";
    }
    sql += " ORDER BY created_at DESC";
    
    auto stmt = prepare_statement(sql.c_str());
    std::vector<Screenshot> screenshots;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        screenshots.push_back(read_screenshot_from_stmt(stmt));
    }
    
    sqlite3_finalize(stmt);
    return screenshots;
}

// 保存截图
bool Screenshot::save() {
    sqlite3* db = Database::get_instance().get_handle();
    if (!db) return false;

    // 检查记录是否已存在
    const char* check_sql = "SELECT id FROM screenshots WHERE filename = ? AND filepath = ? LIMIT 1";
    auto check_stmt = prepare_statement(check_sql);
    sqlite3_bind_text(check_stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(check_stmt, 2, filepath.c_str(), -1, SQLITE_STATIC);
    
    bool exists = sqlite3_step(check_stmt) == SQLITE_ROW;
    if (exists) {
        id = sqlite3_column_int64(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    
    const char* sql = exists ? 
        R"(UPDATE screenshots 
           SET filename = ?, filepath = ?, created_at = ?, width = ?, height = ?,
               file_size = ?, metadata = ?, updated_at = ?
           WHERE id = ?)" :
        R"(INSERT INTO screenshots (filename, filepath, created_at, width, height,
                                  file_size, metadata, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?))";
    
    auto stmt = prepare_statement(sql);
    bind_screenshot_fields(stmt, *this, false);
    if (exists) {
        sqlite3_bind_int64(stmt, 9, id);
    }
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (success && !exists) {
        id = sqlite3_last_insert_rowid(db);
    }
    
    sqlite3_finalize(stmt);
    return success;
}

// 删除截图
bool Screenshot::remove() {
    if (id == 0) return false;
    
    const char* sql = "UPDATE screenshots SET deleted_at = ? WHERE id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, TimeUtils::now());
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = execute_statement(stmt, "remove screenshot");
    sqlite3_finalize(stmt);
    return success;
}

// 从指定目录获取截图
std::vector<Screenshot> Screenshot::find_by_directory(const std::wstring& dir_path, int64_t last_id, int limit) {
    try {
        std::vector<Screenshot> screenshots;
        
        if (!std::filesystem::exists(dir_path)) {
            return screenshots;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension();
                if (ext == L".png" || ext == L".jpg" || ext == L".jpeg") {
                    try {
                        auto screenshot = Screenshot::from_file(entry.path());
                        if (last_id == 0 || screenshot.id > last_id) {
                            screenshots.push_back(screenshot);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing file {}: {}", entry.path().string(), e.what());
                    }
                }
            }
        }
        
        std::sort(screenshots.begin(), screenshots.end(), 
                [](const Screenshot& a, const Screenshot& b) {
                    return a.id > b.id;
                });
        
        if (screenshots.size() > static_cast<size_t>(limit)) {
            screenshots.resize(limit);
        }
        
        return screenshots;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error in find_by_directory: {}", e.what());
        throw;
    }
}

// 检查目录中是否有更多截图
bool Screenshot::has_more(const std::wstring& dir_path, int64_t last_id) {
    if (!std::filesystem::exists(dir_path)) {
        return false;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (ec) {
            spdlog::error("Error iterating directory: {}", ec.message());
            continue;
        }

        if (entry.is_regular_file()) {
            auto ext = entry.path().extension();
            if (ext == L".png" || ext == L".jpg" || ext == L".jpeg") {
                try {
                    auto screenshot = Screenshot::from_file(entry.path());
                    if (screenshot.id < last_id) {
                        return true;
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error processing file {}: {}", entry.path().string(), e.what());
                    continue;
                }
            }
        }
    }
    return false;
}

// 从文件创建Screenshot对象
Screenshot Screenshot::from_file(const std::filesystem::path& file_path) {
    Screenshot screenshot;
    try {
        screenshot.filepath = file_path.string();
        screenshot.filename = file_path.filename().string();
        
        // 获取文件时间并转换为Unix时间戳
        auto last_write_time = std::filesystem::last_write_time(file_path);
        auto timestamp = TimeUtils::file_time_to_timestamp(last_write_time);
        
        screenshot.created_at = timestamp;
        screenshot.updated_at = timestamp;
        
        // 生成唯一ID
        screenshot.id = Screenshot::generate_id(screenshot.filepath, screenshot.filename);
        screenshot.file_size = std::filesystem::file_size(file_path);
        
        // 使用WIC读取图片信息
        auto source = ImageProcessor::LoadFromFile(file_path);
        if (!source) {
            throw std::runtime_error("无法读取图片: " + file_path.string());
        }
        
        // 获取图片尺寸
        ImageProcessor::GetImageDimensions(source.Get(), screenshot.width, screenshot.height);
        
        // 设置其他字段的默认值
        screenshot.metadata = "{}";  // 空的JSON对象
        screenshot.thumbnail_generated = false;
        
    } catch (const std::exception& e) {
        spdlog::error("Error creating screenshot from file {}: {}", file_path.string(), e.what());
        throw;
    }
    
    return screenshot;
}

// Album类实现
Album Album::find_by_id(int64_t id) {
    const char* sql = R"(
        SELECT id, name, description, cover_screenshot_id, 
               created_at, updated_at, deleted_at
        FROM screenshot_albums 
        WHERE id = ?
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, id);
    
    Album album;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        album = read_album_from_stmt(stmt);
    }
    
    sqlite3_finalize(stmt);
    return album;
}

// 获取所有相册
std::vector<Album> Album::find_all(bool include_deleted) {
    std::string sql = R"(
        SELECT id, name, description, cover_screenshot_id, 
               created_at, updated_at, deleted_at
        FROM screenshot_albums
    )";
    if (!include_deleted) {
        sql += " WHERE deleted_at IS NULL";
    }
    sql += " ORDER BY created_at DESC";
    
    auto stmt = prepare_statement(sql.c_str());
    std::vector<Album> albums;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        albums.push_back(read_album_from_stmt(stmt));
    }
    
    sqlite3_finalize(stmt);
    return albums;
}

// 保存相册
bool Album::save() {
    sqlite3* db = Database::get_instance().get_handle();
    
    if (id == 0) {
        const char* sql = R"(
            INSERT INTO screenshot_albums (name, description, cover_screenshot_id,
                                        created_at, updated_at)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        auto stmt = prepare_statement(sql);
        bind_album_fields(stmt, *this);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, std::time(nullptr));
        
        bool success = execute_statement(stmt, "insert album");
        if (success) {
            id = sqlite3_last_insert_rowid(db);
        }
        
        sqlite3_finalize(stmt);
        return success;
    } else {
        const char* sql = R"(
            UPDATE screenshot_albums 
            SET name = ?, description = ?, cover_screenshot_id = ?, updated_at = ?
            WHERE id = ?
        )";
        
        auto stmt = prepare_statement(sql);
        bind_album_fields(stmt, *this);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, id);
        
        bool success = execute_statement(stmt, "update album");
        sqlite3_finalize(stmt);
        return success;
    }
}

// 删除相册
bool Album::remove() {
    if (id == 0) return false;
    
    const char* sql = "UPDATE screenshot_albums SET deleted_at = ? WHERE id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, std::time(nullptr));
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = execute_statement(stmt, "remove album");
    sqlite3_finalize(stmt);
    return success;
}

// 添加截图到相册
bool Album::add_screenshot(int64_t screenshot_id, int position) {
    const char* sql = R"(
        INSERT INTO album_screenshots (album_id, screenshot_id, position, created_at)
        VALUES (?, ?, ?, ?)
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    sqlite3_bind_int(stmt, 3, position);
    sqlite3_bind_int64(stmt, 4, std::time(nullptr));
    
    bool success = execute_statement(stmt, "add screenshot to album");
    sqlite3_finalize(stmt);
    return success;
}

// 从相册中移除截图
bool Album::remove_screenshot(int64_t screenshot_id) {
    const char* sql = "DELETE FROM album_screenshots WHERE album_id = ? AND screenshot_id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    
    bool success = execute_statement(stmt, "remove screenshot from album");
    sqlite3_finalize(stmt);
    return success;
}

// 获取相册中的截图
std::vector<Screenshot> Album::get_screenshots() const {
    const char* sql = R"(
        SELECT s.* 
        FROM screenshots s
        JOIN album_screenshots a ON s.id = a.screenshot_id
        WHERE a.album_id = ? AND s.deleted_at IS NULL
        ORDER BY a.position
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, id);
    
    std::vector<Screenshot> screenshots;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        screenshots.push_back(read_screenshot_from_stmt(stmt));
    }
    
    sqlite3_finalize(stmt);
    return screenshots;
} 