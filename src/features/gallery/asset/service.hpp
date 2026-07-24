#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Asset::Service {

// 查询服务
auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string>;

auto query_asset_layout_meta(Core::State::AppState& app_state,
                             const Types::QueryAssetLayoutMetaParams& params)
    -> std::expected<Types::QueryAssetLayoutMetaResponse, std::string>;

auto get_timeline_buckets(Core::State::AppState& app_state,
                          const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string>;

auto get_assets_by_month(Core::State::AppState& app_state,
                         const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string>;

auto get_asset_main_colors(Core::State::AppState& app_state,
                           const Types::GetAssetMainColorsParams& params)
    -> std::expected<std::vector<Types::AssetMainColor>, std::string>;

auto get_home_stats(Core::State::AppState& app_state)
    -> std::expected<Types::HomeStats, std::string>;

auto get_batch_selection_summary(Core::State::AppState& app_state,
                                 const Types::BatchSelectionSummaryParams& params)
    -> std::expected<Types::BatchSelectionSummary, std::string>;

auto update_assets_review_state(Core::State::AppState& app_state,
                                const Types::UpdateAssetsReviewStateParams& params)
    -> std::expected<Types::OperationResult, std::string>;

auto update_asset_description(Core::State::AppState& app_state,
                              const Types::UpdateAssetDescriptionParams& params)
    -> std::expected<Types::OperationResult, std::string>;

auto update_assets_description(Core::State::AppState& app_state,
                               const Types::UpdateAssetsDescriptionParams& params)
    -> std::expected<Types::OperationResult, std::string>;

// 维护服务
auto get_missing_assets(Core::State::AppState& app_state)
    -> std::expected<Types::MissingAssetsResponse, std::string>;

auto purge_missing_assets(Core::State::AppState& app_state,
                          const Types::PurgeMissingAssetsParams& params)
    -> std::expected<Types::PurgeMissingAssetsResult, std::string>;

auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string>;

}  // namespace Features::Gallery::Asset::Service
