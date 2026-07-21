module;

export module Features.Gallery.Color.Repository;

import std;
import Core.State;
import Features.Gallery.Color.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Color::Repository {

// 在调用方已经建立的事务中替换颜色；本函数不单独提交，供资产聚合写入复用。
export auto replace_asset_colors_in_transaction(Core::State::AppState& app_state,
                                                std::int64_t asset_id,
                                                const std::vector<Types::ExtractedColor>& colors)
    -> std::expected<void, std::string>;

export auto get_asset_main_colors(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Features::Gallery::Types::AssetMainColor>, std::string>;

}  // namespace Features::Gallery::Color::Repository
