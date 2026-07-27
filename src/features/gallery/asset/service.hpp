#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::asset::service {

// 查询服务
auto query_assets(core::AppState& app_state, const QueryAssetsParams& params)
    -> std::expected<ListResponse, std::string>;

auto query_asset_layout_meta(core::AppState& app_state, const QueryAssetLayoutMetaParams& params)
    -> std::expected<QueryAssetLayoutMetaResponse, std::string>;

auto get_timeline_buckets(core::AppState& app_state, const TimelineBucketsParams& params)
    -> std::expected<TimelineBucketsResponse, std::string>;

auto get_assets_by_month(core::AppState& app_state, const GetAssetsByMonthParams& params)
    -> std::expected<GetAssetsByMonthResponse, std::string>;

auto get_asset_main_colors(core::AppState& app_state, const GetAssetMainColorsParams& params)
    -> std::expected<std::vector<AssetMainColor>, std::string>;

auto get_home_stats(core::AppState& app_state) -> std::expected<HomeStats, std::string>;

auto get_batch_selection_summary(core::AppState& app_state,
                                 const BatchSelectionSummaryParams& params)
    -> std::expected<BatchSelectionSummary, std::string>;

auto update_assets_review_state(core::AppState& app_state,
                                const UpdateAssetsReviewStateParams& params)
    -> std::expected<OperationResult, std::string>;

auto update_asset_description(core::AppState& app_state, const UpdateAssetDescriptionParams& params)
    -> std::expected<OperationResult, std::string>;

auto update_assets_description(core::AppState& app_state,
                               const UpdateAssetsDescriptionParams& params)
    -> std::expected<OperationResult, std::string>;

// 维护服务
auto get_missing_assets(core::AppState& app_state)
    -> std::expected<MissingAssetsResponse, std::string>;

auto purge_missing_assets(core::AppState& app_state, const PurgeMissingAssetsParams& params)
    -> std::expected<PurgeMissingAssetsResult, std::string>;

auto load_asset_cache(core::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Metadata>, std::string>;

// 只加载指定扫描根下的资产缓存，避免单根扫描读取整个图库。
auto load_asset_cache_under_root(core::AppState& app_state, const std::string& root_path)
    -> std::expected<std::unordered_map<std::string, Metadata>, std::string>;

}  // namespace features::gallery::asset::service
