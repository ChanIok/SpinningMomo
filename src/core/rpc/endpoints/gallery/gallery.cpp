module;

module Core.RPC.Endpoints.Gallery;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery;
import Features.Gallery.Types;
import Core.RPC.Endpoints.Gallery.Asset;
import Core.RPC.Endpoints.Gallery.Tag;
import Core.RPC.Endpoints.Gallery.Folder;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery {

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_directory(Core::State::AppState& app_state,
                           const Features::Gallery::Types::ScanOptions& options)
    -> RpcAwaitable<Features::Gallery::Types::ScanResult> {
  auto result = Features::Gallery::scan_directory(app_state, options);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图 RPC 处理函数 =============

auto handle_cleanup_thumbnails(Core::State::AppState& app_state,
                               [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图统计 RPC 处理函数 =============

auto handle_get_thumbnail_stats(Core::State::AppState& app_state,
                                [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::string> {
  auto result = Features::Gallery::get_thumbnail_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 注册子模块的 RPC 方法
  Asset::register_all(app_state);
  Tag::register_all(app_state);
  Folder::register_all(app_state);

  // 扫描和索引
  register_method<Features::Gallery::Types::ScanOptions, Features::Gallery::Types::ScanResult>(
      app_state, app_state.rpc->registry, "gallery.scanDirectory", handle_scan_directory,
      "Scan directory for asset files and add them to the library. Supports ignore rules and "
      "folder management.");

  // 缩略图操作
  register_method<EmptyParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  register_method<EmptyParams, std::string>(app_state, app_state.rpc->registry,
                                            "gallery.thumbnailStats", handle_get_thumbnail_stats,
                                            "Get thumbnail storage statistics");
}

}  // namespace Core::RPC::Endpoints::Gallery
