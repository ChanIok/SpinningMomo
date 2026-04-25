module;

export module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Features.Gallery.Types;
import <asio.hpp>;

namespace Features::Gallery::Asset::Service {

// 查询服务
export auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string>;

export auto query_asset_layout_meta(Core::State::AppState& app_state,
                                    const Types::QueryAssetLayoutMetaParams& params)
    -> std::expected<Types::QueryAssetLayoutMetaResponse, std::string>;

export auto query_photo_map_points(Core::State::AppState& app_state,
                                   const Types::QueryPhotoMapPointsParams& params)
    -> std::expected<std::vector<Types::PhotoMapPoint>, std::string>;

export auto get_timeline_buckets(Core::State::AppState& app_state,
                                 const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string>;

export auto get_assets_by_month(Core::State::AppState& app_state,
                                const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string>;

export auto get_infinity_nikki_details(Core::State::AppState& app_state,
                                       const Types::GetInfinityNikkiDetailsParams& params)
    -> std::expected<Types::InfinityNikkiDetails, std::string>;

export auto get_asset_main_colors(Core::State::AppState& app_state,
                                  const Types::GetAssetMainColorsParams& params)
    -> std::expected<std::vector<Types::AssetMainColor>, std::string>;

export auto get_home_stats(Core::State::AppState& app_state)
    -> std::expected<Types::HomeStats, std::string>;

export auto update_assets_review_state(Core::State::AppState& app_state,
                                       const Types::UpdateAssetsReviewStateParams& params)
    -> std::expected<Types::OperationResult, std::string>;

export auto update_asset_description(Core::State::AppState& app_state,
                                     const Types::UpdateAssetDescriptionParams& params)
    -> std::expected<Types::OperationResult, std::string>;

export auto set_infinity_nikki_user_record(Core::State::AppState& app_state,
                                           const Types::SetInfinityNikkiUserRecordParams& params)
    -> std::expected<Types::OperationResult, std::string>;

export auto set_infinity_nikki_world_record(Core::State::AppState& app_state,
                                            const Types::SetInfinityNikkiWorldRecordParams& params)
    -> std::expected<Types::OperationResult, std::string>;

export auto get_infinity_nikki_metadata_names(
    Core::State::AppState& app_state, const Types::GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<Types::InfinityNikkiMetadataNames, std::string>>;

// 维护服务
export auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string>;

}  // namespace Features::Gallery::Asset::Service
