module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Asset;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Asset;
import Features.Asset.Types;
import Features.Asset.Repository;

namespace Core::RPC::Endpoints::Asset {

// ============= 资产项管理 RPC 处理函数 =============

auto handle_list_asset(Core::State::AppState& app_state,
                       const Features::Asset::Types::ListParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::ListResponse>> {
  auto result = Features::Asset::Repository::list_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_asset(Core::State::AppState& app_state,
                      const Features::Asset::Types::GetParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::Asset>> {
  auto result = Features::Asset::Repository::get_asset_by_id(app_state, params.id);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  if (!result->has_value()) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::MethodNotFound),
                            .message = "Asset item not found"});
  }

  co_return result->value();
}

auto handle_delete_asset(Core::State::AppState& app_state,
                         const Features::Asset::Types::DeleteParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::OperationResult>> {
  auto result = Features::Asset::delete_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_asset(Core::State::AppState& app_state,
                       const Features::Asset::Types::ScanParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::ScanResult>> {
  auto result = Features::Asset::scan_directories(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图 RPC 处理函数 =============

auto handle_cleanup_thumbnails(Core::State::AppState& app_state,
                               [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::OperationResult>> {
  auto result = Features::Asset::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统计和信息 RPC 处理函数 =============

auto handle_get_asset_stats(Core::State::AppState& app_state,
                            const Features::Asset::Types::GetStatsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::Stats>> {
  auto result = Features::Asset::get_asset_stats(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_thumbnail_stats(Core::State::AppState& app_state,
                                [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::string>> {
  auto result = Features::Asset::get_thumbnail_stats(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 维护和优化 RPC 处理函数 =============

struct CleanupDeletedParams {
  std::optional<int> days_old = 30;
};

auto handle_cleanup_deleted(Core::State::AppState& app_state, const CleanupDeletedParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Asset::Types::OperationResult>> {
  auto result = Features::Asset::cleanup_deleted_assets(app_state, params.days_old.value_or(30));

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 基础资产项操作
  Core::RPC::register_method<Features::Asset::Types::ListParams,
                             Features::Asset::Types::ListResponse>(
      app_state, app_state.rpc->registry, "asset.list", handle_list_asset,
      "Get paginated list of asset items with filtering and sorting");

  Core::RPC::register_method<Features::Asset::Types::GetParams, Features::Asset::Types::Asset>(
      app_state, app_state.rpc->registry, "asset.get", handle_get_asset,
      "Get detailed information about a specific asset item");

  Core::RPC::register_method<Features::Asset::Types::DeleteParams,
                             Features::Asset::Types::OperationResult>(
      app_state, app_state.rpc->registry, "asset.delete", handle_delete_asset,
      "Delete a asset item, optionally including the physical file");

  // 扫描和索引
  Core::RPC::register_method<Features::Asset::Types::ScanParams,
                             Features::Asset::Types::ScanResult>(
      app_state, app_state.rpc->registry, "asset.scan", handle_scan_asset,
      "Scan directories for asset files and add them to the library");

  // 缩略图操作
  Core::RPC::register_method<rfl::Generic, Features::Asset::Types::OperationResult>(
      app_state, app_state.rpc->registry, "asset.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  // 统计信息
  Core::RPC::register_method<Features::Asset::Types::GetStatsParams, Features::Asset::Types::Stats>(
      app_state, app_state.rpc->registry, "asset.stats", handle_get_asset_stats,
      "Get asset library statistics");

  Core::RPC::register_method<rfl::Generic, std::string>(
      app_state, app_state.rpc->registry, "asset.thumbnailStats", handle_get_thumbnail_stats,
      "Get thumbnail storage statistics");

  // 维护操作
  Core::RPC::register_method<CleanupDeletedParams, Features::Asset::Types::OperationResult>(
      app_state, app_state.rpc->registry, "asset.cleanupDeleted", handle_cleanup_deleted,
      "Clean up soft-deleted items older than specified days");

}

}  // namespace Core::RPC::Endpoints::Asset
