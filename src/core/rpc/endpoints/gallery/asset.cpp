module;

module Core.RPC.Endpoints.Gallery.Asset;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.NotificationHub;
import Features.Gallery;
import Features.Gallery.Types;
import Features.Gallery.Asset.Service;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery::Asset {

// ============= 时间线视图 RPC 处理函数 =============

auto handle_get_timeline_buckets(Core::State::AppState& app_state,
                                 const Features::Gallery::Types::TimelineBucketsParams& params)
    -> RpcAwaitable<Features::Gallery::Types::TimelineBucketsResponse> {
  auto result = Features::Gallery::Asset::Service::get_timeline_buckets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_assets_by_month(Core::State::AppState& app_state,
                                const Features::Gallery::Types::GetAssetsByMonthParams& params)
    -> RpcAwaitable<Features::Gallery::Types::GetAssetsByMonthResponse> {
  auto result = Features::Gallery::Asset::Service::get_assets_by_month(app_state, params);

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
  auto result = Features::Gallery::Asset::Service::query_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_query_photo_map_points(
    Core::State::AppState& app_state,
    const Features::Gallery::Types::QueryPhotoMapPointsParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::PhotoMapPoint>> {
  auto result = Features::Gallery::Asset::Service::query_photo_map_points(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_infinity_nikki_photo_params(
    Core::State::AppState& app_state,
    const Features::Gallery::Types::GetInfinityNikkiPhotoParamsParams& params)
    -> RpcAwaitable<std::optional<Features::Gallery::Types::InfinityNikkiPhotoParams>> {
  auto result =
      Features::Gallery::Asset::Service::get_infinity_nikki_photo_params(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_asset_main_colors(Core::State::AppState& app_state,
                                  const Features::Gallery::Types::GetAssetMainColorsParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::AssetMainColor>> {
  auto result = Features::Gallery::Asset::Service::get_asset_main_colors(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_home_stats(Core::State::AppState& app_state,
                           [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<Features::Gallery::Types::HomeStats> {
  auto result = Features::Gallery::Asset::Service::get_home_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 资产动作 RPC 处理函数 =============

auto handle_open_asset_default(Core::State::AppState& app_state,
                               const Features::Gallery::Types::GetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::open_asset_with_default_app(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_reveal_asset_in_explorer(Core::State::AppState& app_state,
                                     const Features::Gallery::Types::GetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::reveal_asset_in_explorer(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_move_assets_to_trash(Core::State::AppState& app_state,
                                 const Features::Gallery::Types::AssetIdsParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::move_assets_to_trash(app_state, params.ids);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_update_assets_review_state(
    Core::State::AppState& app_state,
    const Features::Gallery::Types::UpdateAssetsReviewStateParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::Asset::Service::update_assets_review_state(app_state, params);

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

  register_method<Features::Gallery::Types::QueryPhotoMapPointsParams,
                  std::vector<Features::Gallery::Types::PhotoMapPoint>>(
      app_state, app_state.rpc->registry, "gallery.queryPhotoMapPoints",
      handle_query_photo_map_points,
      "Query Infinity Nikki photo map points using the current gallery filters");

  register_method<Features::Gallery::Types::GetInfinityNikkiPhotoParamsParams,
                  std::optional<Features::Gallery::Types::InfinityNikkiPhotoParams>>(
      app_state, app_state.rpc->registry, "gallery.getInfinityNikkiPhotoParams",
      handle_get_infinity_nikki_photo_params,
      "Get extracted Infinity Nikki photo parameters for the specified asset");

  register_method<Features::Gallery::Types::GetAssetMainColorsParams,
                  std::vector<Features::Gallery::Types::AssetMainColor>>(
      app_state, app_state.rpc->registry, "gallery.getAssetMainColors",
      handle_get_asset_main_colors, "Get extracted main colors for the specified asset");

  register_method<EmptyParams, Features::Gallery::Types::HomeStats>(
      app_state, app_state.rpc->registry, "gallery.getHomeStats", handle_get_home_stats,
      "Get home page gallery stats summary");

  register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.openAssetDefault", handle_open_asset_default,
      "Open the selected asset file with the default system application");

  register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.revealAssetInExplorer",
      handle_reveal_asset_in_explorer, "Reveal and select the asset file in explorer");

  register_method<Features::Gallery::Types::AssetIdsParams,
                  Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.moveAssetsToTrash", handle_move_assets_to_trash,
      "Move selected asset files to system recycle bin and remove them from gallery index");

  register_method<Features::Gallery::Types::UpdateAssetsReviewStateParams,
                  Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateAssetsReviewState",
      handle_update_assets_review_state,
      "Batch update Lightroom-style review metadata such as rating and pick/reject state");
}

}  // namespace Core::RPC::Endpoints::Gallery::Asset
