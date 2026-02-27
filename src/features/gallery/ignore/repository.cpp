module;

module Features.Gallery.Ignore.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.State;
import Core.Database.Types;
import Features.Gallery.Types;
import Utils.Logger;
import <rfl.hpp>;

namespace Features::Gallery::Ignore::Repository {

// ============= 基本 CRUD 操作 =============

auto create_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
    INSERT INTO ignore_rules (
      folder_id, rule_pattern, pattern_type, rule_type, 
      is_enabled, description, created_at, updated_at
    ) VALUES (?, ?, ?, ?, ?, ?, 
      strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
      strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    )
  )";

  std::vector<Core::Database::Types::DbParam> params = {
      rule.folder_id.has_value() ? Core::Database::Types::DbParam{rule.folder_id.value()}
                                 : Core::Database::Types::DbParam{std::monostate{}},
      rule.rule_pattern,
      rule.pattern_type,
      rule.rule_type,
      rule.is_enabled,
      rule.description.has_value() ? Core::Database::Types::DbParam{rule.description.value()}
                                   : Core::Database::Types::DbParam{std::monostate{}}};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to create ignore rule: " + result.error());
  }

  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result || !id_result->has_value()) {
    return std::unexpected("Failed to get inserted rule ID");
  }

  Logger().info("Created ignore rule with ID {}: {}", id_result->value(), rule.rule_pattern);
  return id_result->value();
}

auto get_ignore_rule_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE id = ?
  )";

  auto result = Core::Database::query<Types::IgnoreRule>(*app_state.database, sql, {id});
  if (!result) {
    return std::unexpected("Failed to query ignore rule: " + result.error());
  }

  if (result->empty()) {
    return std::nullopt;
  }

  return std::make_optional(result->at(0));
}

auto update_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE ignore_rules 
    SET folder_id = ?, rule_pattern = ?, pattern_type = ?, rule_type = ?,
        is_enabled = ?, description = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ?
  )";

  std::vector<Core::Database::Types::DbParam> params = {
      rule.folder_id.has_value() ? Core::Database::Types::DbParam{rule.folder_id.value()}
                                 : Core::Database::Types::DbParam{std::monostate{}},
      rule.rule_pattern,
      rule.pattern_type,
      rule.rule_type,
      rule.is_enabled,
      rule.description.has_value() ? Core::Database::Types::DbParam{rule.description.value()}
                                   : Core::Database::Types::DbParam{std::monostate{}},
      rule.id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update ignore rule: " + result.error());
  }

  Logger().debug("Updated ignore rule ID {}", rule.id);
  return {};
}

auto delete_ignore_rule(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "DELETE FROM ignore_rules WHERE id = ?";

  auto result = Core::Database::execute(*app_state.database, sql, {id});
  if (!result) {
    return std::unexpected("Failed to delete ignore rule: " + result.error());
  }

  Logger().info("Deleted ignore rule ID {}", id);
  return {};
}

// ============= 基于文件夹的查询操作 =============

auto get_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE folder_id = ? AND is_enabled = 1
    ORDER BY created_at ASC
  )";

  auto result = Core::Database::query<Types::IgnoreRule>(*app_state.database, sql, {folder_id});
  if (!result) {
    return std::unexpected("Failed to query rules by folder_id: " + result.error());
  }

  return std::move(result.value());
}

auto get_rules_by_directory_path(Core::State::AppState& app_state,
                                 const std::string& directory_path)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string> {
  // 先查找folder_id
  std::string folder_sql = "SELECT id FROM folders WHERE path = ?";
  auto folder_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, folder_sql, {directory_path});

  if (!folder_result) {
    return std::unexpected("Failed to query folder by path: " + folder_result.error());
  }

  if (!folder_result->has_value()) {
    return std::vector<Types::IgnoreRule>{};  // 文件夹不存在，返回空列表
  }

  return get_rules_by_folder_id(app_state, folder_result->value());
}

auto get_global_rules(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE folder_id IS NULL AND is_enabled = 1
    ORDER BY created_at ASC
  )";

  auto result = Core::Database::query<Types::IgnoreRule>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to query global rules: " + result.error());
  }

  return std::move(result.value());
}

// ============= 批量操作 =============

auto replace_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id,
                                const std::vector<Types::ScanIgnoreRule>& scan_rules)
    -> std::expected<void, std::string> {
  auto transaction_result = Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        // 先删除该文件夹已有规则，再插入新的完整规则集
        auto delete_result = Core::Database::execute(
            db_state, "DELETE FROM ignore_rules WHERE folder_id = ?", {folder_id});
        if (!delete_result) {
          return std::unexpected("Failed to delete existing rules: " + delete_result.error());
        }

        for (const auto& scan_rule : scan_rules) {
          if (scan_rule.pattern.empty()) {
            continue;
          }

          std::string insert_sql = R"(
            INSERT INTO ignore_rules (
              folder_id, rule_pattern, pattern_type, rule_type, 
              is_enabled, description, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, 
              strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
              strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
            )
          )";

          std::vector<Core::Database::Types::DbParam> params = {
              folder_id,
              scan_rule.pattern,
              scan_rule.pattern_type,
              scan_rule.rule_type,
              1,
              scan_rule.description.has_value()
                  ? Core::Database::Types::DbParam{scan_rule.description.value()}
                  : Core::Database::Types::DbParam{std::monostate{}}};

          auto insert_result = Core::Database::execute(db_state, insert_sql, params);
          if (!insert_result) {
            return std::unexpected("Failed to insert ignore rule: " + insert_result.error());
          }
        }

        return {};
      });

  if (!transaction_result) {
    return std::unexpected("Transaction failed: " + transaction_result.error());
  }

  Logger().info("Replaced ignore rules for folder_id {} with {} rule(s)", folder_id,
                scan_rules.size());
  return {};
}

auto batch_update_ignore_rules(Core::State::AppState& app_state,
                               const std::vector<Types::IgnoreRule>& rules)
    -> std::expected<void, std::string> {
  if (rules.empty()) {
    return {};
  }

  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        for (const auto& rule : rules) {
          auto update_result = update_ignore_rule(app_state, rule);
          if (!update_result) {
            return std::unexpected("Failed to update rule ID " + std::to_string(rule.id) + ": " +
                                   update_result.error());
          }
        }

        return {};
      });
}

auto delete_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string> {
  std::string sql = "DELETE FROM ignore_rules WHERE folder_id = ?";

  auto result = Core::Database::execute(*app_state.database, sql, {folder_id});
  if (!result) {
    return std::unexpected("Failed to delete rules by folder_id: " + result.error());
  }

  // 获取删除的行数
  auto count_result = Core::Database::query_scalar<int>(*app_state.database, "SELECT changes()");
  int deleted_count = count_result && count_result->has_value() ? count_result->value() : 0;

  Logger().info("Deleted {} ignore rules for folder_id {}", deleted_count, folder_id);
  return deleted_count;
}

// ============= 规则管理和维护 =============

auto toggle_rule_enabled(Core::State::AppState& app_state, std::int64_t id, bool enabled)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE ignore_rules 
    SET is_enabled = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ?
  )";

  auto result = Core::Database::execute(*app_state.database, sql, {enabled ? 1 : 0, id});
  if (!result) {
    return std::unexpected("Failed to toggle rule enabled status: " + result.error());
  }

  Logger().debug("Toggled ignore rule ID {} to {}", id, enabled ? "enabled" : "disabled");
  return {};
}

auto cleanup_orphaned_rules(Core::State::AppState& app_state) -> std::expected<int, std::string> {
  std::string sql = R"(
    DELETE FROM ignore_rules 
    WHERE folder_id IS NOT NULL 
      AND folder_id NOT IN (SELECT id FROM folders)
  )";

  auto result = Core::Database::execute(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to cleanup orphaned rules: " + result.error());
  }

  auto count_result = Core::Database::query_scalar<int>(*app_state.database, "SELECT changes()");
  int deleted_count = count_result && count_result->has_value() ? count_result->value() : 0;

  if (deleted_count > 0) {
    Logger().info("Cleaned up {} orphaned ignore rules", deleted_count);
  }

  return deleted_count;
}

auto count_rules(Core::State::AppState& app_state, std::optional<std::int64_t> folder_id)
    -> std::expected<int, std::string> {
  std::string sql = "SELECT COUNT(*) FROM ignore_rules WHERE is_enabled = 1";
  std::vector<Core::Database::Types::DbParam> params;

  if (folder_id.has_value()) {
    sql += " AND folder_id = ?";
    params.push_back(folder_id.value());
  }

  auto result = Core::Database::query_scalar<int>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to count ignore rules: " + result.error());
  }

  return result->value_or(0);
}

}  // namespace Features::Gallery::Ignore::Repository
