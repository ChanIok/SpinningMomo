module;

export module Extensions.InfinityNikki.MetadataDict;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;
import <asio.hpp>;

namespace Extensions::InfinityNikki::MetadataDict {

export auto resolve_metadata_names(Core::State::AppState& app_state,
                                   const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>>;

}  // namespace Extensions::InfinityNikki::MetadataDict
