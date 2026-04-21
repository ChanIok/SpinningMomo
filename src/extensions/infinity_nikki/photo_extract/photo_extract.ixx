module;

#include <asio.hpp>

export module Extensions.InfinityNikki.PhotoExtract;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;

namespace Extensions::InfinityNikki::PhotoExtract {

export auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>>;

export auto extract_photo_params_silent_incremental(
    Core::State::AppState& app_state, const InfinityNikkiSilentExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>>;

}  // namespace Extensions::InfinityNikki::PhotoExtract
