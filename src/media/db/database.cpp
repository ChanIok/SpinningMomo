#include "database.hpp"

Database& Database::get_instance() {
    static Database instance;
    return instance;
}

void Database::init(const std::string& db_path) {
    // 如果数据库已经初始化，直接返回
    if (db_handle_) {
        return;
    }
    
    // 打开数据库连接
    int rc = sqlite3_open(db_path.c_str(), &db_handle_);
    if (rc != SQLITE_OK) {
        std::string error_msg = sqlite3_errmsg(db_handle_);
        sqlite3_close(db_handle_);
        db_handle_ = nullptr;
        throw DatabaseException("数据库打开失败: " + error_msg);
    }
    
    // 启用外键约束
    execute("PRAGMA foreign_keys = ON;");
    
    // 创建数据表（如果不存在）
    execute(R"(
        -- 截图表
        CREATE TABLE IF NOT EXISTS screenshots (
            id INTEGER PRIMARY KEY,           -- 主键ID
            filename TEXT NOT NULL,           -- 文件名
            filepath TEXT NOT NULL,           -- 文件路径
            created_at DATETIME NOT NULL,     -- 创建时间
            width INTEGER NOT NULL,           -- 图片宽度
            height INTEGER NOT NULL,          -- 图片高度
            file_size INTEGER NOT NULL,       -- 文件大小(bytes)
            metadata TEXT,                    -- 元数据(JSON格式)
            thumbnail_generated INTEGER DEFAULT 0,  -- 缩略图是否已生成
            deleted_at DATETIME,              -- 软删除时间
            updated_at DATETIME NOT NULL      -- 更新时间
        );
        
        -- 创建截图表索引
        CREATE INDEX IF NOT EXISTS idx_screenshots_created_at ON screenshots(created_at);
        CREATE INDEX IF NOT EXISTS idx_screenshots_deleted_at ON screenshots(deleted_at);
        
        -- 相册表
        CREATE TABLE IF NOT EXISTS screenshot_albums (
            id INTEGER PRIMARY KEY,           -- 主键ID
            name TEXT NOT NULL,               -- 相册名称
            description TEXT,                 -- 相册描述
            cover_screenshot_id INTEGER,      -- 封面截图ID
            created_at DATETIME NOT NULL,     -- 创建时间
            updated_at DATETIME NOT NULL,     -- 更新时间
            deleted_at DATETIME,              -- 软删除时间
            FOREIGN KEY (cover_screenshot_id) REFERENCES screenshots(id)
        );
        
        -- 创建相册表索引
        CREATE INDEX IF NOT EXISTS idx_screenshot_albums_created_at ON screenshot_albums(created_at);
        CREATE INDEX IF NOT EXISTS idx_screenshot_albums_deleted_at ON screenshot_albums(deleted_at);
        
        -- 相册-截图关联表
        CREATE TABLE IF NOT EXISTS album_screenshots (
            album_id INTEGER NOT NULL,        -- 相册ID
            screenshot_id INTEGER NOT NULL,    -- 截图ID
            position INTEGER NOT NULL,        -- 截图在相册中的位置
            created_at DATETIME NOT NULL,     -- 添加时间
            FOREIGN KEY (album_id) REFERENCES screenshot_albums(id) ON DELETE CASCADE,
            FOREIGN KEY (screenshot_id) REFERENCES screenshots(id) ON DELETE CASCADE,
            PRIMARY KEY (album_id, screenshot_id)
        );
        
        -- 创建关联表索引
        CREATE INDEX IF NOT EXISTS idx_album_screenshots_album_id ON album_screenshots(album_id);
        CREATE INDEX IF NOT EXISTS idx_album_screenshots_screenshot_id ON album_screenshots(screenshot_id);
        CREATE INDEX IF NOT EXISTS idx_album_screenshots_position ON album_screenshots(position);
    )");
}

void Database::close() {
    if (db_handle_) {
        sqlite3_close(db_handle_);
        db_handle_ = nullptr;
    }
}

Database::~Database() {
    close();
}

void Database::execute(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_handle_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "未知错误";
        sqlite3_free(error_msg);
        throw DatabaseException("SQL执行错误: " + error);
    }
} 