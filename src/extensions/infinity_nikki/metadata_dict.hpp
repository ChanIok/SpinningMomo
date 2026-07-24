#pragma once

#include <asio.hpp>

#include "core/state/app_state.hpp"
#include "extensions/infinity_nikki/types.hpp"

namespace Extensions::InfinityNikki::MetadataDict {

auto resolve_metadata_names(Core::State::AppState& app_state,
                            const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>>;

}  // namespace Extensions::InfinityNikki::MetadataDict
