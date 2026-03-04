module;

#include <asio.hpp>

module Core.RPC.Endpoints.Gallery;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.NotificationHub;
import Core.Async;
import Core.Tasks;
import Features.Gallery;
import Features.Gallery.Types;
import Core.RPC.Endpoints.Gallery.Asset;
import Core.RPC.Endpoints.Gallery.Tag;
import Core.RPC.Endpoints.Gallery.Folder;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery {

struct StartScanDirectoryResult {
  std::string task_id;
};

auto launch_scan_directory_task(Core::State::AppState& app_state,
                                const Features::Gallery::Types::ScanOptions& options,
                                const std::string& task_id) -> void {
  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, options, task_id]() -> asio::awaitable<void> {
        // 确保此协程先立即让出执行权，这样 RPC 可以先返回 task_id，
        // 而不会被后续同步的扫描流水线阻塞。
        co_await asio::post(asio::use_awaitable);

        Core::Tasks::mark_task_running(app_state, task_id);

        auto progress_callback =
            [&app_state, &task_id](const Features::Gallery::Types::ScanProgress& progress) {
              Core::Tasks::TaskProgress task_progress{
                  .stage = progress.stage,
                  .current = progress.current,
                  .total = progress.total,
                  .percent = progress.percent,
                  .message = progress.message,
              };
              Core::Tasks::update_task_progress(app_state, task_id, task_progress);
            };

        auto scan_result = Features::Gallery::scan_directory(app_state, options, progress_callback);
        if (!scan_result) {
          auto error_message = "Asset scan failed: " + scan_result.error();
          Logger().error("{}", error_message);
          Core::Tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }

        const auto& result = scan_result.value();
        Core::Tasks::update_task_progress(
            app_state, task_id,
            Core::Tasks::TaskProgress{
                .stage = "completed",
                .current = result.total_files,
                .total = result.total_files,
                .percent = 100.0,
                .message =
                    std::format("Scanned {}, new {}, updated {}, deleted {}", result.total_files,
                                result.new_items, result.updated_items, result.deleted_items),
            });
        Core::Tasks::complete_task_success(app_state, task_id);
        Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
      },
      asio::detached);
}

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

auto handle_start_scan_directory(Core::State::AppState& app_state,
                                 const Features::Gallery::Types::ScanOptions& options)
    -> RpcAwaitable<StartScanDirectoryResult> {
  if (Core::Tasks::has_active_task_of_type(app_state, "gallery.scanDirectory")) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::InvalidRequest),
                                       .message = "Another gallery scan task is already running"});
  }

  auto task_id = Core::Tasks::create_task(app_state, "gallery.scanDirectory", options.directory);
  if (task_id.empty()) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to create gallery scan task"});
  }

  launch_scan_directory_task(app_state, options, task_id);

  co_return StartScanDirectoryResult{.task_id = task_id};
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

  register_method<Features::Gallery::Types::ScanOptions, StartScanDirectoryResult>(
      app_state, app_state.rpc->registry, "gallery.startScanDirectory", handle_start_scan_directory,
      "Create a background scan task for the gallery and return task id immediately.");

  // 缩略图操作
  register_method<EmptyParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  register_method<EmptyParams, std::string>(app_state, app_state.rpc->registry,
                                            "gallery.thumbnailStats", handle_get_thumbnail_stats,
                                            "Get thumbnail storage statistics");
}

}  // namespace Core::RPC::Endpoints::Gallery
