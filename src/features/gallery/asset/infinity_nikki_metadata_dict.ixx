module;

#include <asio.hpp>

export module Features.Gallery.Asset.InfinityNikkiMetadataDict;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::InfinityNikkiMetadataDict {

export auto resolve_metadata_names(Core::State::AppState& app_state,
                                   const Types::GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<Types::InfinityNikkiMetadataNames, std::string>>;

}  // namespace Features::Gallery::Asset::InfinityNikkiMetadataDict
