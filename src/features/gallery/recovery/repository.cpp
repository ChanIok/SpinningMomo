module;

module Features.Gallery.Recovery.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Recovery.Types;

namespace Features::Gallery::Recovery::Repository {

auto get_state_by_root_path(Core::State::AppState& app_state, const std::string& root_path)
    -> std::expected<std::optional<Types::WatchRootRecoveryState>, std::string> {
  // 按 root_path 查询上次保存的恢复检查点。
  std::string sql = R"(
    SELECT root_path, volume_identity, journal_id, checkpoint_usn, rule_fingerprint, updated_at
    FROM watch_root_recovery_state
    WHERE root_path = ?
  )";

  auto result = Core::Database::query_single<Types::WatchRootRecoveryState>(*app_state.database,
                                                                            sql, {root_path});
  if (!result) {
    return std::unexpected("Failed to query watch root recovery state: " + result.error());
  }

  return result.value();
}

auto upsert_state(Core::State::AppState& app_state, const Types::WatchRootRecoveryState& state)
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

  std::vector<Core::Database::Types::DbParam> params = {
      state.root_path,
      state.volume_identity,
      state.journal_id.has_value() ? Core::Database::Types::DbParam{state.journal_id.value()}
                                   : Core::Database::Types::DbParam{std::monostate{}},
      state.checkpoint_usn.has_value()
          ? Core::Database::Types::DbParam{state.checkpoint_usn.value()}
          : Core::Database::Types::DbParam{std::monostate{}},
      state.rule_fingerprint,
  };

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to upsert watch root recovery state: " + result.error());
  }

  return {};
}

auto delete_state_by_root_path(Core::State::AppState& app_state, const std::string& root_path)
    -> std::expected<void, std::string> {
  // root 被移除时清理对应的恢复状态。
  auto result = Core::Database::execute(*app_state.database,
                                        "DELETE FROM watch_root_recovery_state WHERE root_path = ?",
                                        {root_path});
  if (!result) {
    return std::unexpected("Failed to delete watch root recovery state: " + result.error());
  }

  return {};
}

}  // namespace Features::Gallery::Recovery::Repository
