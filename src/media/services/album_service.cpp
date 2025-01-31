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
        throw std::runtime_error("相册名称不能为空");
    }

    Album album;
    album.name = name;
    album.description = description;
    
    if (!album.save()) {
        throw std::runtime_error("创建相册失败");
    }
    
    return album;
}

std::vector<Album> AlbumService::get_albums(bool include_deleted) {
    return Album::find_all(include_deleted);
}

Album AlbumService::get_album(int64_t id) {
    Album album = Album::find_by_id(id);
    if (album.id == 0) {
        throw std::runtime_error("相册不存在");
    }
    return album;
}

bool AlbumService::update_album(Album& album) {
    if (album.id == 0) {
        return false;
    }
    return album.save();
}

bool AlbumService::delete_album(int64_t id) {
    Album album = Album::find_by_id(id);
    if (album.id == 0) {
        return false;
    }
    return album.remove();
}

bool AlbumService::add_screenshot_to_album(int64_t album_id, int64_t screenshot_id) {
    Album album = Album::find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    
    // 获取最后的位置编号并加1
    int position = get_last_position(album_id) + 1;
    return album.add_screenshot(screenshot_id, position);
}

bool AlbumService::remove_screenshot_from_album(int64_t album_id, int64_t screenshot_id) {
    Album album = Album::find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    return album.remove_screenshot(screenshot_id);
}

std::vector<Screenshot> AlbumService::get_album_screenshots(int64_t album_id) {
    Album album = Album::find_by_id(album_id);
    if (album.id == 0) {
        throw std::runtime_error("相册不存在");
    }
    return album.get_screenshots();
}

bool AlbumService::set_album_cover(int64_t album_id, int64_t screenshot_id) {
    Album album = Album::find_by_id(album_id);
    if (album.id == 0) {
        return false;
    }
    
    album.cover_screenshot_id = screenshot_id;
    return album.save();
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