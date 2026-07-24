#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::OriginalLocator {

auto make_root_host_name(std::int64_t root_id) -> std::wstring;

auto populate_asset_locators(Core::State::AppState& app_state, std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string>;

auto resolve_original_file_path(Core::State::AppState& app_state, std::int64_t root_id,
                                std::string_view relative_path)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Features::Gallery::OriginalLocator
