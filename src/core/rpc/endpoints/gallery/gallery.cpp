#include "core/rpc/endpoints/gallery/gallery.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/async/async.hpp"
#include "core/rpc/endpoints/gallery/asset.hpp"
#include "core/rpc/endpoints/gallery/download.hpp"
#include "core/rpc/endpoints/gallery/folder.hpp"
#include "core/rpc/endpoints/gallery/tag.hpp"
#include "core/rpc/notification_hub.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/tasks/tasks.hpp"
#include "features/gallery/gallery.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"

namespace core::rpc::endpoints::gallery {

struct StartScanDirectoryResult {
  std::string task_id;
};

auto launch_scan_directory_task(core::AppState& app_state,
                                const features::gallery::ScanOptions& options,
                                const std::string& task_id) -> void {
  auto* io_context = core::async::get_io_context(app_state);
  if (!io_context) {
    core::tasks::complete_task_failed(app_state, task_id, "Async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, options, task_id]() -> asio::awaitable<void> {
        // 确保此协程先立即让出执行权，这样 RPC 可以先返回 task_id，
        // 而不会被后续同步的扫描流水线阻塞。
        co_await asio::post(asio::use_awaitable);

        core::tasks::mark_task_running(app_state, task_id);

        auto progress_callback = [&app_state,
                                  &task_id](const features::gallery::ScanProgress& progress) {
          core::tasks::TaskProgress task_progress{
              .stage = progress.stage,
              .current = progress.current,
              .total = progress.total,
              .percent = progress.percent,
              .message = progress.message,
          };
          core::tasks::update_task_progress(app_state, task_id, task_progress);
        };

        auto scan_result = features::gallery::scan_directory(app_state, options, progress_callback);
        if (!scan_result) {
          auto error_message = "Asset scan failed: " + scan_result.error();
          Logger().error("{}", error_message);
          core::tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }

        const auto& result = scan_result.value();
        core::tasks::update_task_progress(
            app_state, task_id,
            core::tasks::TaskProgress{
                .stage = "completed",
                .current = result.total_files,
                .total = result.total_files,
                .percent = 100.0,
                .message =
                    std::format("Scanned {}, new {}, updated {}, missing {}", result.total_files,
                                result.new_items, result.updated_items, result.missing_items),
            });
        core::tasks::complete_task_success(app_state, task_id);
        core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
      },
      core::async::log_completion("Gallery scan task"));
}

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_directory(core::AppState& app_state, const features::gallery::ScanOptions& options)
    -> RpcAwaitable<features::gallery::ScanResult> {
  auto result = features::gallery::scan_directory(app_state, options);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_start_scan_directory(core::AppState& app_state,
                                 const features::gallery::ScanOptions& options)
    -> RpcAwaitable<StartScanDirectoryResult> {
  if (core::tasks::has_active_task_of_type(app_state, "gallery.scanDirectory")) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::InvalidRequest),
                                       .message = "Another gallery scan task is already running"});
  }

  auto task_id = core::tasks::create_task(app_state, "gallery.scanDirectory", options.directory);
  if (task_id.empty()) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to create gallery scan task"});
  }

  launch_scan_directory_task(app_state, options, task_id);

  co_return StartScanDirectoryResult{.task_id = task_id};
}

// ============= 缩略图 RPC 处理函数 =============

auto handle_cleanup_thumbnails(core::AppState& app_state,
                               [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图统计 RPC 处理函数 =============

auto handle_get_thumbnail_stats(core::AppState& app_state,
                                [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::string> {
  auto result = features::gallery::get_thumbnail_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(core::AppState& app_state) -> void {
  // 缩略图统计和清理只操作应用管理的数据，因此允许 LAN 调用。
  // 注册子模块的 RPC 方法
  asset::register_all(app_state);
  download::register_all(app_state);
  tag::register_all(app_state);
  folder::register_all(app_state);

  // 扫描和索引
  register_method<features::gallery::ScanOptions, features::gallery::ScanResult>(
      app_state, app_state.rpc->registry, "gallery.scanDirectory", handle_scan_directory,
      "Scan directory for asset files and add them to the library. Supports ignore rules and "
      "folder management.");

  register_method<features::gallery::ScanOptions, StartScanDirectoryResult>(
      app_state, app_state.rpc->registry, "gallery.startScanDirectory", handle_start_scan_directory,
      "Create a background scan task for the gallery and return task id immediately.");

  // 缩略图操作
  register_method<EmptyParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files", AccessLevel::lan);

  register_method<EmptyParams, std::string>(app_state, app_state.rpc->registry,
                                            "gallery.thumbnailStats", handle_get_thumbnail_stats,
                                            "Get thumbnail storage statistics", AccessLevel::lan);
}

}  // namespace core::rpc::endpoints::gallery
