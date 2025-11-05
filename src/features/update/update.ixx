module;

export module Features.Update;

import std;
import Core.State;
import Features.Update.State;
import Features.Update.Types;

namespace Features::Update {

// 初始化Update模块
export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

// 检查更新
export auto check_for_update(Core::State::AppState& app_state)
    -> std::expected<Types::CheckUpdateResult, std::string>;

// 下载更新
export auto download_update(Core::State::AppState& app_state)
    -> std::expected<Types::DownloadUpdateResult, std::string>;

// 安装更新
export auto install_update(Core::State::AppState& app_state,
                           const Types::InstallUpdateParams& params)
    -> std::expected<Types::InstallUpdateResult, std::string>;

// 执行待处理的更新
export auto execute_pending_update(Core::State::AppState& app_state) -> void;

}  // namespace Features::Update