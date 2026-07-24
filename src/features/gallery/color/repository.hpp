#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/color/types.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Color::Repository {

// 在调用方已经建立的事务中替换颜色；本函数不单独提交，供资产聚合写入复用。
auto replace_asset_colors_in_transaction(Core::State::AppState& app_state, std::int64_t asset_id,
                                         const std::vector<Types::ExtractedColor>& colors)
    -> std::expected<void, std::string>;

auto get_asset_main_colors(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Features::Gallery::Types::AssetMainColor>, std::string>;

}  // namespace Features::Gallery::Color::Repository
