#pragma once

#include <sqlite3.h>
#include <string>
#include <memory>
#include <stdexcept>

/**
 * @brief 数据库管理类
 * 使用单例模式管理SQLite3数据库连接
 */
class Database {
public:
    // 获取数据库单例实例
    static Database& get_instance();
    
    // 初始化数据库连接
    void init(const std::string& db_path = "spinningmomo.db");
    
    // 关闭数据库连接
    void close();
    
    // 获取SQLite3数据库句柄
    sqlite3* get_handle() { return db_handle_; }
    
    // 执行SQL语句
    void execute(const std::string& sql);
    
    // 禁止拷贝和赋值
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    Database() = default;
    ~Database();
    
    sqlite3* db_handle_ = nullptr;  // SQLite3数据库句柄
};

/**
 * @brief 数据库异常类
 * 用于处理数据库操作过程中的错误
 */
class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& message) 
        : std::runtime_error(message) {}
}; 