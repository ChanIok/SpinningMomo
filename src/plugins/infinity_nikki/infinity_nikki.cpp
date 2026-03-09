module;

module Plugins.InfinityNikki;

import std;
import Core.State;
import Plugins.InfinityNikki.GameDirectory;
import Plugins.InfinityNikki.PhotoExtract;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki {

auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string> {
  return GameDirectory::get_game_directory();
}

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> std::expected<InfinityNikkiExtractPhotoParamsResult, std::string> {
  return PhotoExtract::extract_photo_params(app_state, request, progress_callback);
}

}  // namespace Plugins::InfinityNikki
