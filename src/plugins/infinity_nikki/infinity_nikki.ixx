module;

export module Plugins.InfinityNikki;

import std;
import Core.State;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki {

// 获取游戏目录的核心函数
export auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

export auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>;

}  // namespace Plugins::InfinityNikki
