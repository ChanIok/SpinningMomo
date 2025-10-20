module;

module Core.RPC.Endpoints.Gallery.Asset;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery::Asset {

// ============= 时间线视图 RPC 处理函数 =============

auto handle_get_timeline_buckets(Core::State::AppState& app_state,
                                 const Features::Gallery::Types::TimelineBucketsParams& params)
    -> RpcAwaitable<Features::Gallery::Types::TimelineBucketsResponse> {
  auto result = Features::Gallery::Asset::Repository::get_timeline_buckets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_assets_by_month(Core::State::AppState& app_state,
                                const Features::Gallery::Types::GetAssetsByMonthParams& params)
    -> RpcAwaitable<Features::Gallery::Types::GetAssetsByMonthResponse> {
  auto result = Features::Gallery::Asset::Repository::get_assets_by_month(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统一查询 RPC 处理函数 =============

auto handle_query_assets(Core::State::AppState& app_state,
                         const Features::Gallery::Types::QueryAssetsParams& params)
    -> RpcAwaitable<Features::Gallery::Types::ListResponse> {
  auto result = Features::Gallery::Asset::Repository::query_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 时间线视图
  register_method<Features::Gallery::Types::TimelineBucketsParams,
                  Features::Gallery::Types::TimelineBucketsResponse>(
      app_state, app_state.rpc->registry, "gallery.getTimelineBuckets", handle_get_timeline_buckets,
      "Get timeline buckets (months) with asset counts for timeline view");

  register_method<Features::Gallery::Types::GetAssetsByMonthParams,
                  Features::Gallery::Types::GetAssetsByMonthResponse>(
      app_state, app_state.rpc->registry, "gallery.getAssetsByMonth", handle_get_assets_by_month,
      "Get all assets for a specific month in timeline view");

  // 统一资产查询接口
  register_method<Features::Gallery::Types::QueryAssetsParams,
                  Features::Gallery::Types::ListResponse>(
      app_state, app_state.rpc->registry, "gallery.queryAssets", handle_query_assets,
      "Unified asset query interface with flexible filters (folder, month, year, type, search) "
      "and optional pagination");
}

}  // namespace Core::RPC::Endpoints::Gallery::Asset
