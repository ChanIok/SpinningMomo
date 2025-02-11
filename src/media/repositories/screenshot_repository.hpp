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

    // 基本查询
    Screenshot find_by_id(int64_t id) override;
    std::vector<Screenshot> find_all(bool include_deleted = false) override;
    bool save(Screenshot& screenshot) override;
    bool remove(int64_t id) override;

    // 检查文件是否存在
    bool exists_by_path(const std::string& filepath);

    // 分页查询
    std::pair<std::vector<Screenshot>, bool> find_paginated_by_folder(
        const std::string& folder_id,
        const std::string& relative_path,
        int64_t last_id,
        int limit
    );
    
    // 获取月份统计信息
    std::vector<MonthStats> get_month_statistics();
    
    // 获取指定月份的照片
    std::pair<std::vector<Screenshot>, bool> find_by_month(
        int year, 
        int month, 
        int64_t last_id, 
        int limit
    );

    // 更新缩略图状态
    bool update_thumbnail_generated(int64_t id, bool generated);

    // 获取相册照片
    std::pair<std::vector<Screenshot>, bool> find_by_album(
        int64_t album_id, 
        int64_t last_id, 
        int limit
    );

    // 统计文件夹中的照片数量
    int count_by_folder(const std::string& folder_id, const std::string& relative_path);

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