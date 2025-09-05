module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Media;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Media;
import Features.Media.Types;
import Features.Media.Database;

namespace Core::RPC::Endpoints::Media {

// ============= 媒体项管理 RPC 处理函数 =============

auto handle_list_media(Core::State::AppState& app_state,
                       const Features::Media::Types::ListMediaParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaListResponse>> {
  auto result = Features::Media::Database::list_media_items(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_media(Core::State::AppState& app_state,
                      const Features::Media::Types::GetMediaParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaItem>> {
  auto result = Features::Media::Database::get_media_item_by_id(app_state, params.id);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  if (!result->has_value()) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::MethodNotFound),
                            .message = "Media item not found"});
  }

  co_return result->value();
}

auto handle_delete_media(Core::State::AppState& app_state,
                         const Features::Media::Types::DeleteMediaParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaOperationResult>> {
  auto result = Features::Media::delete_media_item(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_media(Core::State::AppState& app_state,
                       const Features::Media::Types::ScanMediaParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::ScanResult>> {
  auto result = Features::Media::scan_media_directories(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图 RPC 处理函数 =============

auto handle_get_thumbnail(Core::State::AppState& app_state,
                          const Features::Media::Types::GetThumbnailParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::vector<uint8_t>>> {
  auto result = Features::Media::get_thumbnail_data(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_generate_thumbnails(Core::State::AppState& app_state,
                                [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::ScanResult>> {
  // 使用默认参数生成缺失的缩略图
  auto result = Features::Media::generate_missing_thumbnails(app_state, 400, 400);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_cleanup_thumbnails(Core::State::AppState& app_state,
                               [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaOperationResult>> {
  auto result = Features::Media::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统计和信息 RPC 处理函数 =============

auto handle_get_media_stats(Core::State::AppState& app_state,
                            const Features::Media::Types::GetStatsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaStats>> {
  auto result = Features::Media::get_media_stats(app_state, params);

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
  auto result = Features::Media::get_thumbnail_stats(app_state);

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
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::MediaOperationResult>> {
  auto result = Features::Media::cleanup_deleted_items(app_state, params.days_old.value_or(30));

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_verify_integrity(Core::State::AppState& app_state,
                             [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::ScanResult>> {
  auto result = Features::Media::verify_media_integrity(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_rebuild_thumbnails(Core::State::AppState& app_state,
                               [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Media::Types::ScanResult>> {
  auto result = Features::Media::rebuild_thumbnail_index(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 基础媒体项操作
  Core::RPC::register_method<Features::Media::Types::ListMediaParams,
                             Features::Media::Types::MediaListResponse>(
      app_state, app_state.rpc->registry, "media.list", handle_list_media,
      "Get paginated list of media items with filtering and sorting");

  Core::RPC::register_method<Features::Media::Types::GetMediaParams,
                             Features::Media::Types::MediaItem>(
      app_state, app_state.rpc->registry, "media.get", handle_get_media,
      "Get detailed information about a specific media item");

  Core::RPC::register_method<Features::Media::Types::DeleteMediaParams,
                             Features::Media::Types::MediaOperationResult>(
      app_state, app_state.rpc->registry, "media.delete", handle_delete_media,
      "Delete a media item, optionally including the physical file");

  // 扫描和索引
  Core::RPC::register_method<Features::Media::Types::ScanMediaParams,
                             Features::Media::Types::ScanResult>(
      app_state, app_state.rpc->registry, "media.scan", handle_scan_media,
      "Scan directories for media files and add them to the library");

  // 缩略图操作
  Core::RPC::register_method<Features::Media::Types::GetThumbnailParams, std::vector<uint8_t>>(
      app_state, app_state.rpc->registry, "media.thumbnail", handle_get_thumbnail,
      "Get thumbnail data for a media item");

  Core::RPC::register_method<rfl::Generic, Features::Media::Types::ScanResult>(
      app_state, app_state.rpc->registry, "media.generateThumbnails", handle_generate_thumbnails,
      "Generate missing thumbnails for all photo items");

  Core::RPC::register_method<rfl::Generic, Features::Media::Types::MediaOperationResult>(
      app_state, app_state.rpc->registry, "media.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  // 统计信息
  Core::RPC::register_method<Features::Media::Types::GetStatsParams,
                             Features::Media::Types::MediaStats>(
      app_state, app_state.rpc->registry, "media.stats", handle_get_media_stats,
      "Get media library statistics");

  Core::RPC::register_method<rfl::Generic, std::string>(
      app_state, app_state.rpc->registry, "media.thumbnailStats", handle_get_thumbnail_stats,
      "Get thumbnail storage statistics");

  // 维护操作
  Core::RPC::register_method<CleanupDeletedParams, Features::Media::Types::MediaOperationResult>(
      app_state, app_state.rpc->registry, "media.cleanupDeleted", handle_cleanup_deleted,
      "Clean up soft-deleted items older than specified days");

  Core::RPC::register_method<rfl::Generic, Features::Media::Types::ScanResult>(
      app_state, app_state.rpc->registry, "media.verifyIntegrity", handle_verify_integrity,
      "Verify integrity of media files and thumbnails");

  Core::RPC::register_method<rfl::Generic, Features::Media::Types::ScanResult>(
      app_state, app_state.rpc->registry, "media.rebuildThumbnails", handle_rebuild_thumbnails,
      "Rebuild the entire thumbnail index");
}

}  // namespace Core::RPC::Endpoints::Media
