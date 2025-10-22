module;

export module Features.Updater;

import std;
import Core.State;
import Features.Updater.State;
import Features.Updater.Types;

namespace Features::Updater {

// 初始化Updater模块
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

}  // namespace Features::Updater