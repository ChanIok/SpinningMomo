#include "models.hpp"
#include "database.hpp"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"

// Screenshot类实现
Screenshot Screenshot::find_by_id(int64_t id) {
    Screenshot screenshot;
    sqlite3* db = Database::get_instance().get_handle();
    
    const char* sql = R"(
        SELECT id, filename, filepath, created_at, width, height, 
               file_size, metadata, deleted_at, updated_at
        FROM screenshots 
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("SQL准备失败");
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        screenshot.id = sqlite3_column_int64(stmt, 0);
        screenshot.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        screenshot.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        screenshot.created_at = sqlite3_column_int64(stmt, 3);
        screenshot.width = sqlite3_column_int(stmt, 4);
        screenshot.height = sqlite3_column_int(stmt, 5);
        screenshot.file_size = sqlite3_column_int64(stmt, 6);
        if (auto text = sqlite3_column_text(stmt, 7)) {
            screenshot.metadata = reinterpret_cast<const char*>(text);
        }
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            screenshot.deleted_at = sqlite3_column_int64(stmt, 8);
        }
        screenshot.updated_at = sqlite3_column_int64(stmt, 9);
    }
    
    sqlite3_finalize(stmt);
    return screenshot;
}

std::vector<Screenshot> Screenshot::find_all(bool include_deleted) {
    std::vector<Screenshot> screenshots;
    sqlite3* db = Database::get_instance().get_handle();
    
    std::string sql = R"(
        SELECT id, filename, filepath, created_at, width, height, 
               file_size, metadata, deleted_at, updated_at
        FROM screenshots
    )";
    if (!include_deleted) {
        sql += " WHERE deleted_at IS NULL";
    }
    sql += " ORDER BY created_at DESC";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("SQL准备失败");
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Screenshot screenshot;
        screenshot.id = sqlite3_column_int64(stmt, 0);
        screenshot.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        screenshot.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        screenshot.created_at = sqlite3_column_int64(stmt, 3);
        screenshot.width = sqlite3_column_int(stmt, 4);
        screenshot.height = sqlite3_column_int(stmt, 5);
        screenshot.file_size = sqlite3_column_int64(stmt, 6);
        if (auto text = sqlite3_column_text(stmt, 7)) {
            screenshot.metadata = reinterpret_cast<const char*>(text);
        }
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            screenshot.deleted_at = sqlite3_column_int64(stmt, 8);
        }
        screenshot.updated_at = sqlite3_column_int64(stmt, 9);
        screenshots.push_back(screenshot);
    }
    
    sqlite3_finalize(stmt);
    return screenshots;
}

bool Screenshot::save() {
    sqlite3* db = Database::get_instance().get_handle();
    
    if (id == 0) {
        // 插入新记录
        const char* sql = R"(
            INSERT INTO screenshots (filename, filepath, created_at, width, height,
                                  file_size, metadata, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filepath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, std::time(nullptr));
        sqlite3_bind_int(stmt, 4, width);
        sqlite3_bind_int(stmt, 5, height);
        sqlite3_bind_int64(stmt, 6, file_size);
        sqlite3_bind_text(stmt, 7, metadata.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 8, std::time(nullptr));
        
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        if (success) {
            id = sqlite3_last_insert_rowid(db);
        }
        
        sqlite3_finalize(stmt);
        return success;
    } else {
        // 更新现有记录
        const char* sql = R"(
            UPDATE screenshots 
            SET filename = ?, filepath = ?, width = ?, height = ?,
                file_size = ?, metadata = ?, updated_at = ?
            WHERE id = ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filepath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, width);
        sqlite3_bind_int(stmt, 4, height);
        sqlite3_bind_int64(stmt, 5, file_size);
        sqlite3_bind_text(stmt, 6, metadata.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 7, std::time(nullptr));
        sqlite3_bind_int64(stmt, 8, id);
        
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }
}

bool Screenshot::remove() {
    if (id == 0) return false;
    
    sqlite3* db = Database::get_instance().get_handle();
    const char* sql = "UPDATE screenshots SET deleted_at = ? WHERE id = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, std::time(nullptr));
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Screenshot> Screenshot::find_by_directory(const std::wstring& dir_path, int64_t last_id, int limit) {
    try {
        std::vector<Screenshot> screenshots;
        
        if (!std::filesystem::exists(dir_path)) {
            // spdlog::warn("Directory does not exist: {}", dir_path);
            return screenshots;
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
                        if (last_id == 0 || screenshot.id > last_id) {
                            screenshots.push_back(screenshot);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing file {}: {}", entry.path().string(), e.what());
                        continue;
                    }
                }
            }
        }
        
        // 按ID降序排序（最新的在前）
        std::sort(screenshots.begin(), screenshots.end(), 
                [](const Screenshot& a, const Screenshot& b) {
                    return a.id > b.id;
                });
        
        // 限制返回数量
        if (screenshots.size() > static_cast<size_t>(limit)) {
            screenshots.resize(limit);
        }
        
        return screenshots;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error in find_by_directory: {}", e.what());
        throw;
    }
}

bool Screenshot::has_more(const std::wstring& dir_path, int64_t last_id) {
    if (!std::filesystem::exists(dir_path)) {
        // spdlog::warn("Directory does not exist: {}", dir_path);
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

Screenshot Screenshot::from_file(const std::filesystem::path& file_path) {
    Screenshot screenshot;
    try {
        screenshot.filepath = file_path.string();
        screenshot.filename = file_path.filename().string();
        
        // 使用文件属性作为ID和时间戳
        auto last_write_time = std::filesystem::last_write_time(file_path);
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            last_write_time.time_since_epoch()).count();
        
        screenshot.id = timestamp;  // 使用时间戳作为ID
        screenshot.created_at = timestamp;
        screenshot.updated_at = timestamp;
        
        screenshot.file_size = std::filesystem::file_size(file_path);
        
        // 暂时使用默认值
        screenshot.width = 1920;
        screenshot.height = 1080;
        
    } catch (const std::exception& e) {
        spdlog::error("Error creating screenshot from file {}: {}", file_path.string(), e.what());
        throw;
    }
    
    return screenshot;
}

// Album类实现
Album Album::find_by_id(int64_t id) {
    Album album;
    sqlite3* db = Database::get_instance().get_handle();
    
    const char* sql = R"(
        SELECT id, name, description, cover_screenshot_id, 
               created_at, updated_at, deleted_at
        FROM screenshot_albums 
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("SQL准备失败");
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        album.id = sqlite3_column_int64(stmt, 0);
        album.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (auto text = sqlite3_column_text(stmt, 2)) {
            album.description = reinterpret_cast<const char*>(text);
        }
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            album.cover_screenshot_id = sqlite3_column_int64(stmt, 3);
        }
        album.created_at = sqlite3_column_int64(stmt, 4);
        album.updated_at = sqlite3_column_int64(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            album.deleted_at = sqlite3_column_int64(stmt, 6);
        }
    }
    
    sqlite3_finalize(stmt);
    return album;
}

std::vector<Album> Album::find_all(bool include_deleted) {
    std::vector<Album> albums;
    sqlite3* db = Database::get_instance().get_handle();
    
    std::string sql = R"(
        SELECT id, name, description, cover_screenshot_id, 
               created_at, updated_at, deleted_at
        FROM screenshot_albums
    )";
    if (!include_deleted) {
        sql += " WHERE deleted_at IS NULL";
    }
    sql += " ORDER BY created_at DESC";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("SQL准备失败");
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Album album;
        album.id = sqlite3_column_int64(stmt, 0);
        album.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (auto text = sqlite3_column_text(stmt, 2)) {
            album.description = reinterpret_cast<const char*>(text);
        }
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            album.cover_screenshot_id = sqlite3_column_int64(stmt, 3);
        }
        album.created_at = sqlite3_column_int64(stmt, 4);
        album.updated_at = sqlite3_column_int64(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            album.deleted_at = sqlite3_column_int64(stmt, 6);
        }
        albums.push_back(album);
    }
    
    sqlite3_finalize(stmt);
    return albums;
}

bool Album::save() {
    sqlite3* db = Database::get_instance().get_handle();
    
    if (id == 0) {
        // 插入新记录
        const char* sql = R"(
            INSERT INTO screenshot_albums (name, description, cover_screenshot_id,
                                        created_at, updated_at)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        if (!description.empty()) {
            sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        if (cover_screenshot_id) {
            sqlite3_bind_int64(stmt, 3, *cover_screenshot_id);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, std::time(nullptr));
        
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        if (success) {
            id = sqlite3_last_insert_rowid(db);
        }
        
        sqlite3_finalize(stmt);
        return success;
    } else {
        // 更新现有记录
        const char* sql = R"(
            UPDATE screenshot_albums 
            SET name = ?, description = ?, cover_screenshot_id = ?, updated_at = ?
            WHERE id = ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        if (!description.empty()) {
            sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        if (cover_screenshot_id) {
            sqlite3_bind_int64(stmt, 3, *cover_screenshot_id);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, id);
        
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }
}

bool Album::remove() {
    if (id == 0) return false;
    
    sqlite3* db = Database::get_instance().get_handle();
    const char* sql = "UPDATE screenshot_albums SET deleted_at = ? WHERE id = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, std::time(nullptr));
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Album::add_screenshot(int64_t screenshot_id, int position) {
    sqlite3* db = Database::get_instance().get_handle();
    const char* sql = R"(
        INSERT INTO album_screenshots (album_id, screenshot_id, position, created_at)
        VALUES (?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    sqlite3_bind_int(stmt, 3, position);
    sqlite3_bind_int64(stmt, 4, std::time(nullptr));
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Album::remove_screenshot(int64_t screenshot_id) {
    sqlite3* db = Database::get_instance().get_handle();
    const char* sql = "DELETE FROM album_screenshots WHERE album_id = ? AND screenshot_id = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Screenshot> Album::get_screenshots() const {
    std::vector<Screenshot> screenshots;
    sqlite3* db = Database::get_instance().get_handle();
    
    const char* sql = R"(
        SELECT s.* 
        FROM screenshots s
        JOIN album_screenshots a ON s.id = a.screenshot_id
        WHERE a.album_id = ? AND s.deleted_at IS NULL
        ORDER BY a.position
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("SQL准备失败");
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Screenshot screenshot;
        screenshot.id = sqlite3_column_int64(stmt, 0);
        screenshot.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        screenshot.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        screenshot.created_at = sqlite3_column_int64(stmt, 3);
        screenshot.width = sqlite3_column_int(stmt, 4);
        screenshot.height = sqlite3_column_int(stmt, 5);
        screenshot.file_size = sqlite3_column_int64(stmt, 6);
        if (auto text = sqlite3_column_text(stmt, 7)) {
            screenshot.metadata = reinterpret_cast<const char*>(text);
        }
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            screenshot.deleted_at = sqlite3_column_int64(stmt, 8);
        }
        screenshot.updated_at = sqlite3_column_int64(stmt, 9);
        screenshots.push_back(screenshot);
    }
    
    sqlite3_finalize(stmt);
    return screenshots;
} 