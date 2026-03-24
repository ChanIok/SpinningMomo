module;

export module Features.Gallery.Recovery.Service;

import std;
import Core.State;
export import Features.Gallery.Recovery.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Recovery::Service {

// 判断指定 root 启动时应走 USN 增量还是 FullScan，返回完整的恢复计划。
export auto prepare_startup_recovery(Core::State::AppState& app_state,
                                     const std::filesystem::path& root_path,
                                     const Features::Gallery::Types::ScanOptions& scan_options)
    -> std::expected<Types::StartupRecoveryPlan, std::string>;

// 正常退出时保存恢复检查点（需重新查询 journal 快照）。
export auto persist_recovery_checkpoint(Core::State::AppState& app_state,
                                        const std::filesystem::path& root_path,
                                        const Features::Gallery::Types::ScanOptions& scan_options,
                                        std::optional<std::int64_t> checkpoint_usn = std::nullopt)
    -> std::expected<void, std::string>;

// 启动恢复专用的轻量 persist：plan 中已携带完整快照信息，无需再次查询 journal。
export auto persist_recovery_state(Core::State::AppState& app_state,
                                   const Types::WatchRootRecoveryState& state)
    -> std::expected<void, std::string>;

// 应用退出时批量保存所有已注册 root 的恢复检查点。
export auto persist_registered_root_checkpoints(Core::State::AppState& app_state) -> void;

}  // namespace Features::Gallery::Recovery::Service
