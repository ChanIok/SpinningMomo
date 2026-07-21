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

// 保存已经成功应用的启动恢复边界；边界后的运行期事件允许下次启动幂等重放。
export auto persist_recovery_state(Core::State::AppState& app_state,
                                   const Types::WatchRootRecoveryState& state)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Recovery::Service
