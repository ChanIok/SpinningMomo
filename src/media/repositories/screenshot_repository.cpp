#include "screenshot_repository.hpp"
#include "media/db/database.hpp"
#include "media/utils/time_utils.hpp"
#include "media/utils/string_utils.hpp"
#include <spdlog/spdlog.h>

ScreenshotRepository& ScreenshotRepository::get_instance() {
    static ScreenshotRepository instance;
    return instance;
}

Screenshot ScreenshotRepository::find_by_id(int64_t id) {
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

std::vector<Screenshot> ScreenshotRepository::find_all(bool include_deleted) {
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

bool ScreenshotRepository::save(Screenshot& screenshot) {
    sqlite3* db = Database::get_instance().get_handle();
    if (!db) return false;

    // Check if record exists
    const char* check_sql = "SELECT id FROM screenshots WHERE filename = ? AND filepath = ? LIMIT 1";
    auto check_stmt = prepare_statement(check_sql);
    sqlite3_bind_text(check_stmt, 1, screenshot.filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(check_stmt, 2, screenshot.filepath.c_str(), -1, SQLITE_STATIC);
    
    bool exists = sqlite3_step(check_stmt) == SQLITE_ROW;
    if (exists) {
        screenshot.id = sqlite3_column_int64(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    
    const char* sql = exists ? 
        R"(UPDATE screenshots 
           SET filename = ?, filepath = ?, created_at = ?, width = ?, height = ?,
               file_size = ?, metadata = ?, updated_at = ?, deleted_at = NULL
           WHERE id = ?)" :
        R"(INSERT INTO screenshots (filename, filepath, created_at, width, height,
                                  file_size, metadata, updated_at, deleted_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, NULL))";
    
    auto stmt = prepare_statement(sql);
    bind_screenshot_fields(stmt, screenshot, false);
    if (exists) {
        sqlite3_bind_int64(stmt, 9, screenshot.id);
    }
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (success && !exists) {
        screenshot.id = sqlite3_last_insert_rowid(db);
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool ScreenshotRepository::remove(int64_t id) {
    const char* sql = "UPDATE screenshots SET deleted_at = ? WHERE id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, TimeUtils::now());
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = execute_statement(stmt, "remove screenshot");
    sqlite3_finalize(stmt);
    return success;
}

std::pair<std::vector<Screenshot>, bool> ScreenshotRepository::find_paginated(int64_t last_id, int limit) {
    std::string sql = R"(
        SELECT s.*
        FROM screenshots s
        WHERE deleted_at IS NULL
    )";
    
    if (last_id > 0) {
        sql += R"(
            AND created_at < (
                SELECT created_at 
                FROM screenshots 
                WHERE id = ? AND deleted_at IS NULL
            )
        )";
    }
    
    sql += " ORDER BY created_at DESC LIMIT ? + 1";
    
    auto stmt = prepare_statement(sql.c_str());
    int param_index = 1;
    
    if (last_id > 0) {
        sqlite3_bind_int64(stmt, param_index++, last_id);
    }
    sqlite3_bind_int(stmt, param_index, limit);
    
    std::vector<Screenshot> screenshots;
    while (sqlite3_step(stmt) == SQLITE_ROW && screenshots.size() < static_cast<size_t>(limit)) {
        screenshots.push_back(read_screenshot_from_stmt(stmt));
    }
    
    bool has_more = sqlite3_step(stmt) == SQLITE_ROW;
    
    sqlite3_finalize(stmt);
    return {screenshots, has_more};
}

std::vector<Screenshot> ScreenshotRepository::find_by_directory(const std::wstring& dir_path, int64_t last_id, int limit) {
    std::vector<Screenshot> screenshots;
    
    if (!std::filesystem::exists(dir_path)) {
        return screenshots;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension();
            if (ext == L".png" || ext == L".jpg" || ext == L".jpeg") {
                try {
                    const char* sql = "SELECT * FROM screenshots WHERE filepath = ? LIMIT 1";
                    auto stmt = prepare_statement(sql);
                    std::string filepath = wide_to_utf8(entry.path().wstring());
                    sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
                    
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        auto screenshot = read_screenshot_from_stmt(stmt);
                        if (last_id == 0 || screenshot.id > last_id) {
                            screenshots.push_back(screenshot);
                        }
                    }
                    sqlite3_finalize(stmt);
                } catch (const std::exception& e) {
                    spdlog::error("Error processing file {}: {}", wide_to_utf8(entry.path().wstring()), e.what());
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
}

bool ScreenshotRepository::update_thumbnail_generated(int64_t id, bool generated) {
    const char* sql = "UPDATE screenshots SET thumbnail_generated = ?, updated_at = ? WHERE id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int(stmt, 1, generated ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, TimeUtils::now());
    sqlite3_bind_int64(stmt, 3, id);
    
    bool success = execute_statement(stmt, "update thumbnail generated status");
    sqlite3_finalize(stmt);
    return success;
}

void ScreenshotRepository::bind_screenshot_fields(sqlite3_stmt* stmt, const Screenshot& screenshot, bool include_id) {
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

Screenshot ScreenshotRepository::read_screenshot_from_stmt(sqlite3_stmt* stmt) {
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

sqlite3_stmt* ScreenshotRepository::prepare_statement(const char* sql) {
    sqlite3* db = Database::get_instance().get_handle();
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::string error_msg = sqlite3_errmsg(db);
        spdlog::error("Failed to prepare SQL statement: {}", error_msg);
        throw std::runtime_error("Failed to prepare SQL statement");
    }
    return stmt;
}

bool ScreenshotRepository::execute_statement(sqlite3_stmt* stmt, const std::string& operation) {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error_msg = sqlite3_errmsg(Database::get_instance().get_handle());
        spdlog::error("Failed to execute {}: {}", operation, error_msg);
        return false;
    }
    return true;
} 