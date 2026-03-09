module;

export module Plugins.InfinityNikki.PhotoExtract;

import std;
import Core.State;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki::PhotoExtract {

export auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>;

}  // namespace Plugins::InfinityNikki::PhotoExtract
