#include "features/gallery/ignore/repository.hpp"

#include "vendor/std.hpp"

#include "vendor/rfl.hpp"

#include "core/database/database.hpp"
#include "core/database/state.hpp"
#include "core/database/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/state.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"

namespace features::gallery::ignore::repository {

// ============= 基本 CRUD 操作 =============

auto bump_ignore_rules_version(core::AppState& app_state) -> void {
  // ignore rules 的真实来源仍然是数据库；这个版本号只用于通知 watcher：
  // “你手里的早期过滤缓存可能过期了，下次需要重新加载”。
  if (!app_state.gallery) {
    return;
  }

  app_state.gallery->ignore_rules_version.fetch_add(1, std::memory_order_acq_rel);
}

auto create_ignore_rule(core::AppState& app_state, const IgnoreRule& rule)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
    INSERT INTO ignore_rules (
      folder_id, rule_pattern, pattern_type, rule_type, 
      is_enabled, description, created_at, updated_at
    ) VALUES (?, ?, ?, ?, ?, ?, 
      strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
      strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    )
    RETURNING id
  )";

  std::vector<core::database::DbParam> params = {
      rule.folder_id.has_value() ? core::database::DbParam{rule.folder_id.value()}
                                 : core::database::DbParam{std::monostate{}},
      rule.rule_pattern,
      rule.pattern_type,
      rule.rule_type,
      rule.is_enabled,
      rule.description.has_value() ? core::database::DbParam{rule.description.value()}
                                   : core::database::DbParam{std::monostate{}}};

  auto result = core::database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result || !result->has_value()) {
    return std::unexpected("Failed to create ignore rule: " +
                           (result ? std::string("missing returned ID") : result.error()));
  }

  Logger().info("Created ignore rule with ID {}: {}", result->value(), rule.rule_pattern);
  bump_ignore_rules_version(app_state);
  return result->value();
}

auto get_ignore_rule_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE id = ?
  )";

  auto result = core::database::query<IgnoreRule>(app_state, sql, {id});
  if (!result) {
    return std::unexpected("Failed to query ignore rule: " + result.error());
  }

  if (result->empty()) {
    return std::nullopt;
  }

  return std::make_optional(result->at(0));
}

auto update_ignore_rule(core::AppState& app_state, const IgnoreRule& rule)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE ignore_rules 
    SET folder_id = ?, rule_pattern = ?, pattern_type = ?, rule_type = ?,
        is_enabled = ?, description = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ?
  )";

  std::vector<core::database::DbParam> params = {
      rule.folder_id.has_value() ? core::database::DbParam{rule.folder_id.value()}
                                 : core::database::DbParam{std::monostate{}},
      rule.rule_pattern,
      rule.pattern_type,
      rule.rule_type,
      rule.is_enabled,
      rule.description.has_value() ? core::database::DbParam{rule.description.value()}
                                   : core::database::DbParam{std::monostate{}},
      rule.id};

  auto result = core::database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to update ignore rule: " + result.error());
  }

  Logger().debug("Updated ignore rule ID {}", rule.id);
  bump_ignore_rules_version(app_state);
  return {};
}

auto delete_ignore_rule(core::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "DELETE FROM ignore_rules WHERE id = ?";

  auto result = core::database::execute(app_state, sql, {id});
  if (!result) {
    return std::unexpected("Failed to delete ignore rule: " + result.error());
  }

  Logger().info("Deleted ignore rule ID {}", id);
  bump_ignore_rules_version(app_state);
  return {};
}

// ============= 基于文件夹的查询操作 =============

auto get_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::vector<IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE folder_id = ? AND is_enabled = 1
    ORDER BY created_at ASC
  )";

  auto result = core::database::query<IgnoreRule>(app_state, sql, {folder_id});
  if (!result) {
    return std::unexpected("Failed to query rules by folder_id: " + result.error());
  }

  return std::move(result.value());
}

auto get_rules_by_directory_path(core::AppState& app_state, const std::string& directory_path)
    -> std::expected<std::vector<IgnoreRule>, std::string> {
  // 先查找folder_id
  std::string folder_sql = "SELECT id FROM folders WHERE path = ?";
  auto folder_result =
      core::database::query_scalar<int64_t>(app_state, folder_sql, {directory_path});

  if (!folder_result) {
    return std::unexpected("Failed to query folder by path: " + folder_result.error());
  }

  if (!folder_result->has_value()) {
    return std::vector<IgnoreRule>{};  // 文件夹不存在，返回空列表
  }

  return get_rules_by_folder_id(app_state, folder_result->value());
}

auto get_global_rules(core::AppState& app_state)
    -> std::expected<std::vector<IgnoreRule>, std::string> {
  std::string sql = R"(
    SELECT id, folder_id, rule_pattern, pattern_type, rule_type, 
           is_enabled, description, created_at, updated_at
    FROM ignore_rules 
    WHERE folder_id IS NULL AND is_enabled = 1
    ORDER BY created_at ASC
  )";

  auto result = core::database::query<IgnoreRule>(app_state, sql);
  if (!result) {
    return std::unexpected("Failed to query global rules: " + result.error());
  }

  return std::move(result.value());
}

// ============= 批量操作 =============

auto replace_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id,
                                const std::vector<ScanIgnoreRule>& scan_rules)
    -> std::expected<void, std::string> {
  auto transaction_result = core::database::execute_transaction(
      app_state, [&](core::AppState& txn_app_state) -> std::expected<void, std::string> {
        // 先删除该文件夹已有规则，再插入新的完整规则集
        auto delete_result = core::database::execute(
            txn_app_state, "DELETE FROM ignore_rules WHERE folder_id = ?", {folder_id});
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

          std::vector<core::database::DbParam> params = {
              folder_id,
              scan_rule.pattern,
              scan_rule.pattern_type,
              scan_rule.rule_type,
              1,
              scan_rule.description.has_value()
                  ? core::database::DbParam{scan_rule.description.value()}
                  : core::database::DbParam{std::monostate{}}};

          auto insert_result = core::database::execute(txn_app_state, insert_sql, params);
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
  bump_ignore_rules_version(app_state);
  return {};
}

auto batch_update_ignore_rules(core::AppState& app_state, const std::vector<IgnoreRule>& rules)
    -> std::expected<void, std::string> {
  if (rules.empty()) {
    return {};
  }

  return core::database::execute_transaction(
      app_state, [&](core::AppState& txn_app_state) -> std::expected<void, std::string> {
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

auto delete_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string> {
  std::string sql = "DELETE FROM ignore_rules WHERE folder_id = ? RETURNING id";

  auto result = core::database::query<core::database::ReturningIdRow>(app_state, sql, {folder_id});
  if (!result) {
    return std::unexpected("Failed to delete rules by folder_id: " + result.error());
  }

  auto deleted_count = static_cast<int>(result->size());

  Logger().info("Deleted {} ignore rules for folder_id {}", deleted_count, folder_id);
  if (deleted_count > 0) {
    bump_ignore_rules_version(app_state);
  }
  return deleted_count;
}

// ============= 规则管理和维护 =============

auto toggle_rule_enabled(core::AppState& app_state, std::int64_t id, bool enabled)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE ignore_rules 
    SET is_enabled = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ?
  )";

  auto result = core::database::execute(app_state, sql, {enabled ? 1 : 0, id});
  if (!result) {
    return std::unexpected("Failed to toggle rule enabled status: " + result.error());
  }

  Logger().debug("Toggled ignore rule ID {} to {}", id, enabled ? "enabled" : "disabled");
  bump_ignore_rules_version(app_state);
  return {};
}

auto cleanup_orphaned_rules(core::AppState& app_state) -> std::expected<int, std::string> {
  std::string sql = R"(
    DELETE FROM ignore_rules 
    WHERE folder_id IS NOT NULL 
      AND folder_id NOT IN (SELECT id FROM folders)
    RETURNING id
  )";

  auto result = core::database::query<core::database::ReturningIdRow>(app_state, sql);
  if (!result) {
    return std::unexpected("Failed to cleanup orphaned rules: " + result.error());
  }

  auto deleted_count = static_cast<int>(result->size());

  if (deleted_count > 0) {
    Logger().info("Cleaned up {} orphaned ignore rules", deleted_count);
    bump_ignore_rules_version(app_state);
  }

  return deleted_count;
}

auto count_rules(core::AppState& app_state, std::optional<std::int64_t> folder_id)
    -> std::expected<int, std::string> {
  std::string sql = "SELECT COUNT(*) FROM ignore_rules WHERE is_enabled = 1";
  std::vector<core::database::DbParam> params;

  if (folder_id.has_value()) {
    sql += " AND folder_id = ?";
    params.push_back(folder_id.value());
  }

  auto result = core::database::query_scalar<int>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to count ignore rules: " + result.error());
  }

  return result->value_or(0);
}

}  // namespace features::gallery::ignore::repository
