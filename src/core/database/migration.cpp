module;

module Core.Database.Migration;

import std;
import Core.Database;
import Core.Database.State;
import Core.Database.Types;
import Utils.Logger;
import <SQLiteCpp/SQLiteCpp.h>;

namespace Core::Database::Migration {

const std::vector<MigrationScript> all_migrations = {
    {1,
     "Initialize database",
     {
         R"(
                CREATE TABLE assets (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL,
                    path TEXT NOT NULL UNIQUE,
                    type TEXT NOT NULL CHECK (type IN ('photo', 'video', 'live_photo', 'unknown')),
                    
                    width INTEGER,
                    height INTEGER,
                    size INTEGER,
                    mime_type TEXT,
                    hash TEXT,
                    folder_id INTEGER REFERENCES folders(id) ON DELETE SET NULL,
                    
                    file_created_at INTEGER,
                    file_modified_at INTEGER,
                    
                    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    deleted_at INTEGER
                );
                )",
         "CREATE INDEX idx_assets_path ON assets(path);",
         "CREATE INDEX idx_assets_type ON assets(type);",
         "CREATE INDEX idx_assets_created_at ON assets(created_at);",
         "CREATE INDEX idx_assets_hash ON assets(hash);",
         "CREATE INDEX idx_assets_folder_id ON assets(folder_id);",
         R"(
                CREATE TRIGGER update_assets_updated_at 
                AFTER UPDATE ON assets 
                FOR EACH ROW
                BEGIN 
                    UPDATE assets SET updated_at = (unixepoch('subsec') * 1000)
                    WHERE id = NEW.id;
                END;
                )",
         R"(
                CREATE TABLE folders (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    path TEXT NOT NULL UNIQUE,
                    parent_id INTEGER REFERENCES folders(id) ON DELETE CASCADE,
                    name TEXT NOT NULL,
                    display_name TEXT,
                    cover_asset_id INTEGER,
                    sort_order INTEGER DEFAULT 0,
                    is_hidden BOOLEAN DEFAULT 0,
                    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    
                    FOREIGN KEY (cover_asset_id) REFERENCES assets(id) ON DELETE SET NULL
                );
                )",
         "CREATE INDEX idx_folders_parent_sort ON folders(parent_id, sort_order);",
         "CREATE INDEX idx_folders_path ON folders(path);",
         R"(
                CREATE TRIGGER update_folders_updated_at 
                AFTER UPDATE ON folders 
                FOR EACH ROW
                BEGIN 
                    UPDATE folders SET updated_at = (unixepoch('subsec') * 1000)
                    WHERE id = NEW.id;
                END;
                )",
         R"(
                CREATE TABLE tags (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL,
                    parent_id INTEGER,
                    sort_order INTEGER DEFAULT 0,
                    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    
                    FOREIGN KEY (parent_id) REFERENCES tags(id) ON DELETE CASCADE,
                    UNIQUE(parent_id, name)
                );
                )",
         R"(
                CREATE TABLE asset_tags (
                    asset_id INTEGER NOT NULL,
                    tag_id INTEGER NOT NULL,
                    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    PRIMARY KEY (asset_id, tag_id),
                    FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE,
                    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
                );
                )",
         "CREATE INDEX idx_tags_parent_sort ON tags(parent_id, sort_order);",
         R"(
                CREATE TRIGGER update_tags_updated_at 
                AFTER UPDATE ON tags 
                FOR EACH ROW
                BEGIN 
                    UPDATE tags SET updated_at = (unixepoch('subsec') * 1000)
                    WHERE id = NEW.id;
                END;
                )",
         "CREATE INDEX idx_asset_tags_tag ON asset_tags(tag_id);",
         R"(
                CREATE TABLE ignore_rules (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    
                    -- 如果为 NULL，表示这是一个全局规则
                    -- 如果有值，它将关联到 folders 表中的一个根目录 (即 parent_id IS NULL 的记录)
                    folder_id INTEGER REFERENCES folders(id) ON DELETE CASCADE,
                    
                    -- 规则模式，可以是 glob 或正则表达式
                    rule_pattern TEXT NOT NULL,
                    
                    -- 规则的类型，'glob' 类似于 .gitignore, 'regex' 则是标准的正则表达式
                    pattern_type TEXT NOT NULL CHECK (pattern_type IN ('glob', 'regex')) DEFAULT 'glob',
                    
                    rule_type TEXT NOT NULL CHECK (rule_type IN ('exclude', 'include')) DEFAULT 'exclude',
                    is_enabled BOOLEAN NOT NULL DEFAULT 1,
                    description TEXT,
                    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
                    
                    UNIQUE(folder_id, rule_pattern)
                );
                )",
         "CREATE INDEX idx_ignore_rules_folder_id ON ignore_rules(folder_id);",
         "CREATE INDEX idx_ignore_rules_enabled ON ignore_rules(is_enabled);",
         "CREATE INDEX idx_ignore_rules_pattern_type ON ignore_rules(pattern_type);",
         R"(
                CREATE TRIGGER update_ignore_rules_updated_at 
                AFTER UPDATE ON ignore_rules 
                FOR EACH ROW
                BEGIN 
                    UPDATE ignore_rules SET updated_at = (unixepoch('subsec') * 1000)
                    WHERE id = NEW.id;
                END;
                )"}}
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