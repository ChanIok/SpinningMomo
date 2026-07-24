#pragma once

#include <asio.hpp>

#include "core/state/app_state.hpp"
#include "extensions/infinity_nikki/types.hpp"

namespace Extensions::InfinityNikki::PhotoExtract {

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>>;

auto extract_photo_params_silent_incremental(
    Core::State::AppState& app_state, const InfinityNikkiSilentExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>>;

}  // namespace Extensions::InfinityNikki::PhotoExtract
