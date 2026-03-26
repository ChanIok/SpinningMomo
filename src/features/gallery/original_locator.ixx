module;

export module Features.Gallery.OriginalLocator;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::OriginalLocator {

export auto make_root_host_name(std::int64_t root_id) -> std::wstring;

export auto populate_asset_locators(Core::State::AppState& app_state,
                                    std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string>;

export auto resolve_original_file_path(Core::State::AppState& app_state, std::int64_t root_id,
                                       std::string_view relative_path)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Features::Gallery::OriginalLocator
