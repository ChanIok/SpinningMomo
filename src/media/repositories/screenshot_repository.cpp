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
               file_size = ?, metadata = ?, photo_time = ?, updated_at = ?, 
               thumbnail_generated = ?, deleted_at = NULL
           WHERE id = ?)" :
        R"(INSERT INTO screenshots (filename, filepath, created_at, width, height,
                                  file_size, metadata, photo_time, updated_at, 
                                  thumbnail_generated, deleted_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL))";
    
    auto stmt = prepare_statement(sql);
    bind_screenshot_fields(stmt, screenshot, false);
    if (exists) {
        sqlite3_bind_int64(stmt, 11, screenshot.id);
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
            AND (
                COALESCE(photo_time, created_at) < (
                    SELECT COALESCE(photo_time, created_at)
                    FROM screenshots 
                    WHERE id = ? AND deleted_at IS NULL
                )
                OR (
                    COALESCE(photo_time, created_at) = (
                        SELECT COALESCE(photo_time, created_at)
                        FROM screenshots 
                        WHERE id = ? AND deleted_at IS NULL
                    )
                    AND id < ?
                )
            )
        )";
    }
    
    sql += " ORDER BY COALESCE(photo_time, created_at) DESC, id DESC LIMIT ? + 1";
    
    auto stmt = prepare_statement(sql.c_str());
    int param_index = 1;
    
    if (last_id > 0) {
        sqlite3_bind_int64(stmt, param_index++, last_id);
        sqlite3_bind_int64(stmt, param_index++, last_id);
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
    if (screenshot.photo_time.has_value()) {
        sqlite3_bind_int64(stmt, idx++, screenshot.photo_time.value());
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    sqlite3_bind_int64(stmt, idx++, screenshot.updated_at);
    sqlite3_bind_int(stmt, idx++, screenshot.thumbnail_generated ? 1 : 0);
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
        screenshot.photo_time = sqlite3_column_int64(stmt, idx);
    }
    idx++;
    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        screenshot.deleted_at = sqlite3_column_int64(stmt, idx);
    }
    idx++;
    screenshot.updated_at = sqlite3_column_int64(stmt, idx);
    screenshot.thumbnail_generated = sqlite3_column_int(stmt, idx) == 1;
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

// 获取月份统计信息
std::vector<MonthStats> ScreenshotRepository::get_month_statistics() {
    const char* sql = R"(
        SELECT 
            strftime('%Y', datetime(created_at, 'unixepoch')) as year,
            strftime('%m', datetime(created_at, 'unixepoch')) as month,
            COUNT(*) as count,
            MIN(id) as first_screenshot_id
        FROM screenshots
        WHERE deleted_at IS NULL
        GROUP BY year, month
        ORDER BY year DESC, month DESC
    )";
    
    auto stmt = prepare_statement(sql);
    std::vector<MonthStats> stats;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MonthStats stat;
        stat.year = std::stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        stat.month = std::stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        stat.count = sqlite3_column_int(stmt, 2);
        stat.first_screenshot_id = sqlite3_column_int64(stmt, 3);
        stats.push_back(stat);
    }
    
    sqlite3_finalize(stmt);
    return stats;
}

std::pair<std::vector<Screenshot>, bool> ScreenshotRepository::find_by_month(int year, int month, int64_t last_id, int limit) {
    std::string sql = R"(
        WITH target_screenshots AS (
            SELECT *
            FROM screenshots
            WHERE deleted_at IS NULL
            AND strftime('%Y', datetime(COALESCE(photo_time, created_at), 'unixepoch')) = ?
            AND strftime('%m', datetime(COALESCE(photo_time, created_at), 'unixepoch')) = ?
    )";
    
    if (last_id > 0) {
        sql += R"(
            AND COALESCE(photo_time, created_at) < (
                SELECT COALESCE(photo_time, created_at)
                FROM screenshots 
                WHERE id = ? AND deleted_at IS NULL
            )
        )";
    }
    
    sql += " ORDER BY COALESCE(photo_time, created_at) DESC, id DESC LIMIT ? + 1)";
    sql += R"(
        SELECT * FROM target_screenshots LIMIT ?
    )";
    
    auto stmt = prepare_statement(sql.c_str());
    int param_index = 1;
    
    // Bind year and month
    std::string year_str = std::to_string(year);
    std::string month_str = month < 10 ? "0" + std::to_string(month) : std::to_string(month);
    sqlite3_bind_text(stmt, param_index++, year_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, param_index++, month_str.c_str(), -1, SQLITE_STATIC);
    
    // Bind last_id if provided
    if (last_id > 0) {
        sqlite3_bind_int64(stmt, param_index++, last_id);
    }
    
    // Bind limit for both the inner and outer queries
    sqlite3_bind_int(stmt, param_index++, limit);
    sqlite3_bind_int(stmt, param_index++, limit);
    
    std::vector<Screenshot> screenshots;
    while (sqlite3_step(stmt) == SQLITE_ROW && screenshots.size() < static_cast<size_t>(limit)) {
        screenshots.push_back(read_screenshot_from_stmt(stmt));
    }
    
    bool has_more = sqlite3_step(stmt) == SQLITE_ROW;
    
    sqlite3_finalize(stmt);
    return {screenshots, has_more};
} 