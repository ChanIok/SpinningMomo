#include "features/gallery/recovery/repository.hpp"

#include "vendor/std.hpp"

#include "core/database/database.hpp"
#include "core/database/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/recovery/types.hpp"

namespace features::gallery::recovery::repository {

auto get_state_by_root_path(core::AppState& app_state, const std::string& root_path)
    -> std::expected<std::optional<WatchRootRecoveryState>, std::string> {
  // 按 root_path 查询上次保存的恢复检查点。
  std::string sql = R"(
    SELECT root_path, volume_identity, journal_id, checkpoint_usn, rule_fingerprint, updated_at
    FROM watch_root_recovery_state
    WHERE root_path = ?
  )";

  auto result = core::database::query_single<WatchRootRecoveryState>(app_state, sql, {root_path});
  if (!result) {
    return std::unexpected("Failed to query watch root recovery state: " + result.error());
  }

  return result.value();
}

auto upsert_state(core::AppState& app_state, const WatchRootRecoveryState& state)
    -> std::expected<void, std::string> {
  // root_path 是主键，重复写入时直接覆盖为最新检查点。
  std::string sql = R"(
    INSERT INTO watch_root_recovery_state (
      root_path, volume_identity, journal_id, checkpoint_usn, rule_fingerprint, updated_at
    ) VALUES (?, ?, ?, ?, ?, (unixepoch('subsec') * 1000))
    ON CONFLICT(root_path) DO UPDATE SET
      volume_identity = excluded.volume_identity,
      journal_id = excluded.journal_id,
      checkpoint_usn = excluded.checkpoint_usn,
      rule_fingerprint = excluded.rule_fingerprint,
      updated_at = (unixepoch('subsec') * 1000)
  )";

  std::vector<core::database::DbParam> params = {
      state.root_path,
      state.volume_identity,
      state.journal_id.has_value() ? core::database::DbParam{state.journal_id.value()}
                                   : core::database::DbParam{std::monostate{}},
      state.checkpoint_usn.has_value() ? core::database::DbParam{state.checkpoint_usn.value()}
                                       : core::database::DbParam{std::monostate{}},
      state.rule_fingerprint,
  };

  auto result = core::database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to upsert watch root recovery state: " + result.error());
  }

  return {};
}

auto delete_state_by_root_path(core::AppState& app_state, const std::string& root_path)
    -> std::expected<void, std::string> {
  // root 被移除时清理对应的恢复状态。
  auto result = core::database::execute(
      app_state, "DELETE FROM watch_root_recovery_state WHERE root_path = ?", {root_path});
  if (!result) {
    return std::unexpected("Failed to delete watch root recovery state: " + result.error());
  }

  return {};
}

}  // namespace features::gallery::recovery::repository
