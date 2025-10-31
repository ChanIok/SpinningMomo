module;

export module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::Service {

// 查询服务
export auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string>;

export auto get_timeline_buckets(Core::State::AppState& app_state,
                                 const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string>;

export auto get_assets_by_month(Core::State::AppState& app_state,
                                const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string>;

// 维护服务
export auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string>;

}  // namespace Features::Gallery::Asset::Service
