#include "album_repository.hpp"
#include "media/db/database.hpp"
#include "media/utils/time_utils.hpp"
#include <spdlog/spdlog.h>

AlbumRepository& AlbumRepository::get_instance() {
    static AlbumRepository instance;
    return instance;
}

Album AlbumRepository::find_by_id(int64_t id) {
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

std::vector<Album> AlbumRepository::find_all(bool include_deleted) {
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

bool AlbumRepository::save(Album& album) {
    sqlite3* db = Database::get_instance().get_handle();
    
    if (album.id == 0) {
        const char* sql = R"(
            INSERT INTO screenshot_albums (name, description, cover_screenshot_id,
                                        created_at, updated_at)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        auto stmt = prepare_statement(sql);
        bind_album_fields(stmt, album);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, std::time(nullptr));
        
        bool success = execute_statement(stmt, "insert album");
        if (success) {
            album.id = sqlite3_last_insert_rowid(db);
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
        bind_album_fields(stmt, album);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int64(stmt, 5, album.id);
        
        bool success = execute_statement(stmt, "update album");
        sqlite3_finalize(stmt);
        return success;
    }
}

bool AlbumRepository::remove(int64_t id) {
    const char* sql = "UPDATE screenshot_albums SET deleted_at = ? WHERE id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, std::time(nullptr));
    sqlite3_bind_int64(stmt, 2, id);
    
    bool success = execute_statement(stmt, "remove album");
    sqlite3_finalize(stmt);
    return success;
}

bool AlbumRepository::add_screenshot(int64_t album_id, int64_t screenshot_id, int position) {
    const char* sql = R"(
        INSERT INTO album_screenshots (album_id, screenshot_id, position, created_at)
        VALUES (?, ?, ?, ?)
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, album_id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    sqlite3_bind_int(stmt, 3, position);
    sqlite3_bind_int64(stmt, 4, std::time(nullptr));
    
    bool success = execute_statement(stmt, "add screenshot to album");
    sqlite3_finalize(stmt);
    return success;
}

bool AlbumRepository::remove_screenshot(int64_t album_id, int64_t screenshot_id) {
    const char* sql = "DELETE FROM album_screenshots WHERE album_id = ? AND screenshot_id = ?";
    auto stmt = prepare_statement(sql);
    
    sqlite3_bind_int64(stmt, 1, album_id);
    sqlite3_bind_int64(stmt, 2, screenshot_id);
    
    bool success = execute_statement(stmt, "remove screenshot from album");
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Screenshot> AlbumRepository::get_screenshots(int64_t album_id) const {
    const char* sql = R"(
        SELECT s.* 
        FROM screenshots s
        JOIN album_screenshots a ON s.id = a.screenshot_id
        WHERE a.album_id = ? AND s.deleted_at IS NULL
        ORDER BY a.position
    )";
    
    auto stmt = prepare_statement(sql);
    sqlite3_bind_int64(stmt, 1, album_id);
    
    std::vector<Screenshot> screenshots;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
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
        screenshots.push_back(screenshot);
    }
    
    sqlite3_finalize(stmt);
    return screenshots;
}

void AlbumRepository::bind_album_fields(sqlite3_stmt* stmt, const Album& album, bool include_id) {
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

Album AlbumRepository::read_album_from_stmt(sqlite3_stmt* stmt) {
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

sqlite3_stmt* AlbumRepository::prepare_statement(const char* sql) const {
    sqlite3* db = Database::get_instance().get_handle();
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::string error_msg = sqlite3_errmsg(db);
        spdlog::error("Failed to prepare SQL statement: {}", error_msg);
        throw std::runtime_error("Failed to prepare SQL statement");
    }
    return stmt;
}

bool AlbumRepository::execute_statement(sqlite3_stmt* stmt, const std::string& operation) {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error_msg = sqlite3_errmsg(Database::get_instance().get_handle());
        spdlog::error("Failed to execute {}: {}", operation, error_msg);
        return false;
    }
    return true;
} 