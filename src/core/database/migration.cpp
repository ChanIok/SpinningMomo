module;

#include <SQLiteCpp/SQLiteCpp.h>

module Core.Database.Migration;

import std;
import Core.Database;
import Core.Database.State;
import Core.Database.Types;
import Utils.Logger;

namespace Core::Database::Migration {
// 所有迁移脚本 - 您只需要在这里添加新的迁移
// 注意：在实际项目中，这个列表可能会从外部配置文件或数据库中加载
const std::vector<MigrationScript> all_migrations = {
    {1,
     "创建 assets 表",
     {
         R"(
                CREATE TABLE assets (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    filename TEXT NOT NULL,
                    filepath TEXT NOT NULL UNIQUE,
                    relative_path TEXT,
                    type TEXT NOT NULL CHECK (type IN ('photo', 'video', 'live_photo', 'unknown')),
                    
                    -- 基本信息
                    width INTEGER,
                    height INTEGER,
                    file_size INTEGER,
                    mime_type TEXT,
                    file_hash TEXT,  -- xxh3哈希值，用于快速比对和去重
                    
                    -- 时间信息
                    created_at TEXT DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
                    updated_at TEXT DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
                    deleted_at TEXT
                );
                )",
         "CREATE INDEX idx_assets_filepath ON assets(filepath);",
         "CREATE INDEX idx_assets_type ON assets(type);",
         "CREATE INDEX idx_assets_created_at ON assets(created_at);",
         "CREATE INDEX idx_assets_file_hash ON assets(file_hash);"}}
    // 添加新迁移时，只需要在这里加新的 MigrationScript 即可
};

// 执行迁移的主函数
auto run_migrations(State::DatabaseState& db_state) -> bool {
  Logger().info("Starting database migration check...");

  // 确保 schema_version 表存在
  std::string create_schema_table_sql = R"(
                CREATE TABLE IF NOT EXISTS schema_version (
                    version INTEGER PRIMARY KEY,
                    description TEXT,
                    applied_at TEXT NOT NULL
                );
            )";
  if (auto result = Core::Database::execute(db_state, create_schema_table_sql); !result) {
    Logger().error("Failed to create schema_version table: {}", result.error());
    return false;
  }

  // 查询当前版本
  std::string select_version_sql = "SELECT MAX(version) FROM schema_version";
  auto version_result = Core::Database::query_scalar<int>(db_state, select_version_sql);

  if (!version_result) {
    Logger().error("Failed to query schema version: {}", version_result.error());
    return false;
  }

  // 如果表为空, query_scalar 返回的 optional 将不包含值, value_or(0) 将返回 0
  int current_version = version_result->value_or(0);

  int latest_version = all_migrations.empty() ? 0 : all_migrations.back().version;

  Logger().info("Current database version: {}, Latest version: {}", current_version,
                latest_version);

  if (current_version >= latest_version) {
    Logger().info("Database is already up to date");
    return true;
  }

  // 执行需要的迁移
  for (const auto& migration : all_migrations) {
    if (migration.version > current_version) {
      Logger().info("Executing migration: Version {} - {}", migration.version,
                    migration.description);

      // 使用新的事务管理方法
      auto migration_result = Core::Database::execute_transaction(
          db_state,
          [&migration](
              State::DatabaseState& transaction_db_state) -> std::expected<void, std::string> {
            // 执行迁移 SQL
            for (const auto& sql : migration.sqls) {
              if (auto result = Core::Database::execute(transaction_db_state, sql); !result) {
                return std::unexpected("Failed to execute migration SQL: " + sql +
                                       " Error: " + result.error());
              }
            }

            // 更新 schema_version 表
            std::string timestamp =
                std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());

            std::vector<Core::Database::Types::DbParam> params = {
                migration.version,
                migration.description,
                timestamp,
            };

            std::string insert_version_sql =
                "INSERT INTO schema_version (version, description, applied_at) VALUES (?, ?, ?)";

            if (auto result =
                    Core::Database::execute(transaction_db_state, insert_version_sql, params);
                !result) {
              return std::unexpected("Failed to update schema_version table: " + result.error());
            }

            return {};  // 成功
          });

      if (migration_result) {
        Logger().info("Migration successful: Version {}", migration.version);
      } else {
        Logger().error("Migration failed for version {}. Error: {}", migration.version,
                       migration_result.error());
        return false;
      }
    }
  }

  Logger().info("All migrations executed successfully");
  return true;
}
}  // namespace Core::Database::Migration