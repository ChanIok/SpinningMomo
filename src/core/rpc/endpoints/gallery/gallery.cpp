module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Gallery;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;

namespace Core::RPC::Endpoints::Gallery {

// ============= 资产项管理 RPC 处理函数 =============

auto handle_list_asset(Core::State::AppState& app_state,
                       const Features::Gallery::Types::ListParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::ListResponse>> {
  auto result = Features::Gallery::Asset::Repository::list_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_asset(Core::State::AppState& app_state,
                      const Features::Gallery::Types::GetParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::Asset>> {
  auto result = Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.id);

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
                         const Features::Gallery::Types::DeleteParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::delete_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_directory(Core::State::AppState& app_state,
                           const Features::Gallery::Types::ScanParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::ScanResult>> {
   Features::Gallery::Types::ScanOptions options{
      .directory = params.directory,
      .generate_thumbnails = params.generate_thumbnails,
      .thumbnail_max_width = params.thumbnail_max_width,
      .thumbnail_max_height = params.thumbnail_max_height,
      .supported_extensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"},
      .ignore_rules = params.ignore_rules,
      .create_folder_records = params.create_folder_records,
      .update_folder_counts = params.update_folder_counts};

  auto result = Features::Gallery::scan_directory(app_state, options);

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
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统计和信息 RPC 处理函数 =============

auto handle_get_asset_stats(Core::State::AppState& app_state,
                            const Features::Gallery::Types::GetStatsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::Stats>> {
  auto result = Features::Gallery::get_asset_stats(app_state, params);

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
  auto result = Features::Gallery::get_thumbnail_stats(app_state);

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
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::cleanup_deleted_assets(app_state, params.days_old.value_or(30));

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
  Core::RPC::register_method<Features::Gallery::Types::ListParams,
                             Features::Gallery::Types::ListResponse>(
      app_state, app_state.rpc->registry, "gallery.list", handle_list_asset,
      "Get paginated list of asset items with filtering and sorting");

  Core::RPC::register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::Asset>(
      app_state, app_state.rpc->registry, "gallery.get", handle_get_asset,
      "Get detailed information about a specific asset item");

  Core::RPC::register_method<Features::Gallery::Types::DeleteParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.delete", handle_delete_asset,
      "Delete a asset item, optionally including the physical file");

  // 扫描和索引
  Core::RPC::register_method<Features::Gallery::Types::ScanParams,
                             Features::Gallery::Types::ScanResult>(
      app_state, app_state.rpc->registry, "gallery.scan", handle_scan_directory,
      "Scan directory for asset files and add them to the library. Supports ignore rules and "
      "folder management.");

  // 缩略图操作
  Core::RPC::register_method<rfl::Generic, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  // 统计信息
  Core::RPC::register_method<Features::Gallery::Types::GetStatsParams,
                             Features::Gallery::Types::Stats>(
      app_state, app_state.rpc->registry, "gallery.stats", handle_get_asset_stats,
      "Get asset library statistics");

  Core::RPC::register_method<rfl::Generic, std::string>(
      app_state, app_state.rpc->registry, "gallery.thumbnailStats", handle_get_thumbnail_stats,
      "Get thumbnail storage statistics");

  // 维护操作
  Core::RPC::register_method<CleanupDeletedParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupDeleted", handle_cleanup_deleted,
      "Clean up soft-deleted items older than specified days");
}

}  // namespace Core::RPC::Endpoints::Gallery
