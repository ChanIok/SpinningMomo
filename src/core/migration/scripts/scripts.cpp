module;

module Core.Migration.Scripts;

import std;
import Core.State;
import Core.Database;
import Core.Migration.Schema;
import Utils.Logger;

namespace Core::Migration::Scripts {

// 执行 SQL Schema 迁移的辅助函数
template <typename SchemaModule>
auto execute_sql_schema(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  for (const auto& sql : SchemaModule::statements) {
    auto result = Core::Database::execute(*app_state.database, std::string(sql));
    if (!result) {
      return std::unexpected(std::format("SQL execution failed: {}", result.error()));
    }
  }
  return {};
}

// Migration: 1.0.0.0 - Initialize database schema
auto migrate_v1_0_0_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 1.0.0.0: Initialize database schema");

  // 首次启动，初始化数据库Schema
  // Settings会在后续initialize时自动创建最新配置
  auto result = execute_sql_schema<Core::Migration::Schema::V001>(app_state);

  if (!result) {
    return std::unexpected("Failed to initialize database schema: " + result.error());
  }

  Logger().info("Database schema initialized successfully");
  return {};
}

// 未来的迁移脚本在此添加
// 示例：
// auto migrate_v2_0_0_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
//   Logger().info("Executing migration to 2.0.0.0");
//   // 迁移逻辑
//   return {};
// }

auto get_all_migrations() -> const std::vector<MigrationScript>& {
  static const std::vector<MigrationScript> migrations = {
      {"1.0.0.0", "Initialize database schema", migrate_v1_0_0_0},

      // 未来版本的迁移脚本在此添加
      // {"2.0.0.0", "Add user preferences", migrate_v2_0_0_0},
  };
  return migrations;
}

}  // namespace Core::Migration::Scripts
