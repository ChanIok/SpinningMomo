module;

export module Features.Gallery.Color.Repository;

import std;
import Core.State;
import Features.Gallery.Color.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Color::Repository {

export struct ColorReplaceBatchItem {
  std::int64_t asset_id = 0;
  std::vector<Types::ExtractedColor> colors;
};

export auto replace_asset_colors(Core::State::AppState& app_state, std::int64_t asset_id,
                                 const std::vector<Types::ExtractedColor>& colors)
    -> std::expected<void, std::string>;

export auto batch_replace_asset_colors(Core::State::AppState& app_state,
                                       const std::vector<ColorReplaceBatchItem>& items)
    -> std::expected<void, std::string>;

export auto get_asset_main_colors(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Features::Gallery::Types::AssetMainColor>, std::string>;

}  // namespace Features::Gallery::Color::Repository
