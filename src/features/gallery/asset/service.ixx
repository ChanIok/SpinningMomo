module;

export module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::Service {

// 查询服务
export auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string>;

export auto query_photo_map_points(Core::State::AppState& app_state,
                                   const Types::QueryPhotoMapPointsParams& params)
    -> std::expected<std::vector<Types::PhotoMapPoint>, std::string>;

export auto get_timeline_buckets(Core::State::AppState& app_state,
                                 const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string>;

export auto get_assets_by_month(Core::State::AppState& app_state,
                                const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string>;

export auto get_infinity_nikki_photo_params(Core::State::AppState& app_state,
                                            const Types::GetInfinityNikkiPhotoParamsParams& params)
    -> std::expected<std::optional<Types::InfinityNikkiPhotoParams>, std::string>;

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

// 维护服务
export auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string>;

}  // namespace Features::Gallery::Asset::Service
