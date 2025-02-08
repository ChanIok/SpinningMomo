#include "album_service.hpp"
#include "media/db/database.hpp"
#include <sqlite3.h>
#include <stdexcept>

AlbumService& AlbumService::get_instance() {
    static AlbumService instance;
    return instance;
}

Album AlbumService::create_album(const std::string& name, const std::string& description) {
    if (name.empty()) {
        throw std::runtime_error("Album name cannot be empty");
    }

    Album album;
    album.name = name;
    album.description = description;
    
    if (!repository_.save(album)) {
        throw std::runtime_error("Failed to create album");
    }
    
    return album;
}

std::vector<Album> AlbumService::get_albums(bool include_deleted) {
    return repository_.find_all(include_deleted);
}

Album AlbumService::get_album(int64_t id) {
    Album album = repository_.find_by_id(id);
    if (album.id == 0) {
        throw std::runtime_error("Album not found");
    }
    return album;
}

bool AlbumService::update_album(Album& album) {
    if (album.id == 0) {
        return false;
    }
    return repository_.save(album);
}

bool AlbumService::delete_album(int64_t id) {
    return repository_.remove(id);
}

bool AlbumService::add_screenshot_to_album(int64_t album_id, int64_t screenshot_id) {
    Album album = repository_.find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    
    // 获取最后的位置编号并加1
    int position = get_last_position(album_id) + 1;
    return repository_.add_screenshot(album_id, screenshot_id, position);
}

bool AlbumService::remove_screenshot_from_album(int64_t album_id, int64_t screenshot_id) {
    Album album = repository_.find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    return repository_.remove_screenshot(album_id, screenshot_id);
}

std::vector<Screenshot> AlbumService::get_album_screenshots(int64_t album_id) {
    Album album = repository_.find_by_id(album_id);
    if (album.id == 0) {
        throw std::runtime_error("Album not found");
    }
    return repository_.get_screenshots(album_id);
}

bool AlbumService::set_album_cover(int64_t album_id, int64_t screenshot_id) {
    Album album = repository_.find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    
    album.cover_screenshot_id = screenshot_id;
    return repository_.save(album);
}

bool AlbumService::add_screenshots_to_album(int64_t album_id, const std::vector<int64_t>& screenshot_ids) {
    Album album = repository_.find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    
    int position = get_last_position(album_id);
    bool success = true;
    
    for (const auto& screenshot_id : screenshot_ids) {
        if (!repository_.add_screenshot(album_id, screenshot_id, ++position)) {
            success = false;
            break;
        }
    }
    
    return success;
}

int AlbumService::get_last_position(int64_t album_id) {
    sqlite3* db = Database::get_instance().get_handle();
    const char* sql = R"(
        SELECT MAX(position) FROM album_screenshots WHERE album_id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int64(stmt, 1, album_id);
    
    int position = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        position = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return position;
} 