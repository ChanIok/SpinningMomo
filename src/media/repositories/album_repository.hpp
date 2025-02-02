#pragma once

#include "base_repository.hpp"
#include "media/db/models.hpp"
#include <sqlite3.h>
#include <string>
#include <vector>

class AlbumRepository : public IRepository<Album, int64_t> {
public:
    static AlbumRepository& get_instance();

    // IRepository 接口实现
    Album find_by_id(int64_t id) override;
    std::vector<Album> find_all(bool include_deleted = false) override;
    bool save(Album& album) override;
    bool remove(int64_t id) override;

    // 相册特有操作
    bool add_screenshot(int64_t album_id, int64_t screenshot_id, int position);
    bool remove_screenshot(int64_t album_id, int64_t screenshot_id);
    std::vector<Screenshot> get_screenshots(int64_t album_id) const;

private:
    AlbumRepository() = default;
    ~AlbumRepository() = default;
    
    // 禁止拷贝
    AlbumRepository(const AlbumRepository&) = delete;
    AlbumRepository& operator=(const AlbumRepository&) = delete;

    // 数据库辅助方法
    void bind_album_fields(sqlite3_stmt* stmt, const Album& album, bool include_id = false);
    Album read_album_from_stmt(sqlite3_stmt* stmt);
    sqlite3_stmt* prepare_statement(const char* sql) const;
    bool execute_statement(sqlite3_stmt* stmt, const std::string& operation);
}; 