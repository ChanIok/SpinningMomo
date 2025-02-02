#pragma once

#include "base_repository.hpp"
#include "media/db/models.hpp"
#include <sqlite3.h>
#include <string>
#include <utility>
#include <filesystem>

class ScreenshotRepository : public IRepository<Screenshot, int64_t> {
public:
    static ScreenshotRepository& get_instance();

    // IRepository implementation
    Screenshot find_by_id(int64_t id) override;
    std::vector<Screenshot> find_all(bool include_deleted = false) override;
    bool save(Screenshot& screenshot) override;
    bool remove(int64_t id) override;

    // Additional specific methods
    std::pair<std::vector<Screenshot>, bool> find_paginated(int64_t last_id, int limit);
    std::vector<Screenshot> find_by_directory(const std::wstring& dir_path, int64_t last_id = 0, int limit = 20);
    bool update_thumbnail_generated(int64_t id, bool generated);

private:
    ScreenshotRepository() = default;
    ~ScreenshotRepository() = default;
    
    // Prevent copying
    ScreenshotRepository(const ScreenshotRepository&) = delete;
    ScreenshotRepository& operator=(const ScreenshotRepository&) = delete;

    // Database helper methods
    void bind_screenshot_fields(sqlite3_stmt* stmt, const Screenshot& screenshot, bool include_id = false);
    Screenshot read_screenshot_from_stmt(sqlite3_stmt* stmt);
    sqlite3_stmt* prepare_statement(const char* sql);
    bool execute_statement(sqlite3_stmt* stmt, const std::string& operation);
}; 