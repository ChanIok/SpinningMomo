module;

#include <asio.hpp>

// TaskService 实现：把无限暖暖相关的重活挂到 Asio 协程上，并对接 Core::Tasks（前端「后台任务」）。
// 约定：对外只暴露 start_* / schedule_silent_*；launch_* 为内部 co_spawn
// 入口，不校验「是否已有同类任务」 （重复校验在各自的 start_* 里通过 has_active_task_of_type +
// create_task 完成）。
module Extensions.InfinityNikki.TaskService;

import std;
import Core.Async;
import Core.RPC.NotificationHub;
import Core.State;
import Core.Tasks;
import Features.Gallery;
import Features.Gallery.Types;
import Features.Settings;
import Features.Settings.State;
import Extensions.InfinityNikki.PhotoExtract;
import Extensions.InfinityNikki.ScreenshotHardlinks;
import Extensions.InfinityNikki.Types;
import Utils.Logger;

namespace Extensions::InfinityNikki::TaskService {

// 与前端 AppHeader / taskStore 的 type 字段一致，勿随意改名。
constexpr auto kInitialScanTaskType = "extensions.infinityNikki.initialScan";
constexpr auto kExtractPhotoParamsTaskType = "extensions.infinityNikki.extractPhotoParams";
constexpr auto kInitializeScreenshotHardlinksTaskType =
    "extensions.infinityNikki.initializeScreenshotHardlinks";

// 硬链接初始化进度上报节流，避免 task.updated 过于频繁。
constexpr auto kProgressEmitInterval = std::chrono::milliseconds(250);
// 任务失败摘要里附带的明细条数上限。
constexpr std::size_t kMaxTaskErrorDetails = 3;

// --- 内部辅助（错误文案、进度映射）---

// 手动按文件夹解析时，UID 必须为纯数字字符串。
auto is_numeric_uid(std::string_view uid) -> bool {
  return !uid.empty() &&
         std::ranges::all_of(uid, [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

// 图库 scan_directory 的进度结构 -> 任务系统统一字段。
auto make_task_progress(const Features::Gallery::Types::ScanProgress& progress)
    -> Core::Tasks::TaskProgress {
  return Core::Tasks::TaskProgress{
      .stage = progress.stage,
      .current = progress.current,
      .total = progress.total,
      .percent = progress.percent,
      .message = progress.message,
  };
}

// 任务失败时 errorMessage 尾部追加若干条明细，避免单条消息过长。
auto append_task_error_details(std::string& message, const std::vector<std::string>& errors)
    -> void {
  if (errors.empty()) {
    return;
  }

  message += " Details: ";

  auto detail_count = std::min(errors.size(), kMaxTaskErrorDetails);
  for (std::size_t i = 0; i < detail_count; ++i) {
    if (i > 0) {
      message += " | ";
    }
    message += errors[i];
  }

  if (errors.size() > detail_count) {
    message += std::format(" | ... and {} more", errors.size() - detail_count);
  }
}

auto make_screenshot_hardlinks_task_error_message(
    const Extensions::InfinityNikki::InfinityNikkiInitializeScreenshotHardlinksResult& summary)
    -> std::string {
  auto message = std::format(
      "Infinity Nikki screenshot hardlinks initialize failed: encountered {} error(s) while "
      "processing {} source file(s)",
      summary.errors.size(), summary.source_count);
  append_task_error_details(message, summary.errors);
  return message;
}

// ---------------------------------------------------------------------------
// launch_*：已由 start_* 创建好 task_id，此处只负责丢进 io_context 并驱动到完成/失败。

auto launch_initial_scan_task(
    Core::State::AppState& app_state, const Features::Gallery::Types::ScanOptions& options,
    const std::string& task_id,
    std::function<void(const Features::Gallery::Types::ScanResult&)> post_scan_callback) -> void {
  if (!app_state.async) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async state is not initialized");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, options, task_id,
       post_scan_callback = std::move(post_scan_callback)]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);

        Core::Tasks::mark_task_running(app_state, task_id);

        auto progress_callback =
            [&app_state, &task_id](const Features::Gallery::Types::ScanProgress& progress) {
              Core::Tasks::update_task_progress(app_state, task_id, make_task_progress(progress));
            };

        auto scan_result = Features::Gallery::scan_directory(app_state, options, progress_callback);
        if (!scan_result) {
          auto error_message = "Infinity Nikki initial scan failed: " + scan_result.error();
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

        if (post_scan_callback) {
          post_scan_callback(result);
        }
      },
      asio::detached);
}

// 走 PhotoExtract::extract_photo_params；有进度回调写任务；成功发
// gallery.changed；部分失败整任务算失败。
auto launch_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request,
    const std::string& task_id) -> void {
  if (!app_state.async) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async state is not initialized");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, request, task_id]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);
        Core::Tasks::mark_task_running(app_state, task_id);

        auto progress_callback =
            [&app_state,
             &task_id](const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsProgress&
                           progress) {
              Core::Tasks::TaskProgress task_progress{
                  .stage = progress.stage,
                  .current = progress.current,
                  .total = progress.total,
                  .percent = progress.percent,
                  .message = progress.message,
              };
              Core::Tasks::update_task_progress(app_state, task_id, task_progress);
            };

        auto extract_result =
            co_await Extensions::InfinityNikki::PhotoExtract::extract_photo_params(
                app_state, request, progress_callback);
        if (!extract_result) {
          auto error_message =
              "Infinity Nikki photo params extract failed: " + extract_result.error();
          Logger().error("{}", error_message);
          Core::Tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }

        const auto& summary = extract_result.value();
        Core::Tasks::update_task_progress(
            app_state, task_id,
            Core::Tasks::TaskProgress{
                .stage = "completed",
                .current = summary.processed_count,
                .total = summary.candidate_count,
                .percent = 100.0,
                .message =
                    std::format("Candidates {}, processed {}, saved {}, skipped {}, failed {}",
                                summary.candidate_count, summary.processed_count,
                                summary.saved_count, summary.skipped_count, summary.failed_count),
            });

        if (summary.saved_count > 0) {
          Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
        }

        if (summary.failed_count > 0) {
          auto warning_message = std::format(
              "Infinity Nikki photo params extract completed with warnings: failed {} / "
              "processed {}",
              summary.failed_count, summary.processed_count);
          append_task_error_details(warning_message, summary.errors);
          Logger().warn("{}", warning_message);
        }

        Core::Tasks::complete_task_success(app_state, task_id);
      },
      asio::detached);
}

// 与 launch_extract_photo_params_task 共用同一套解析逻辑，但不绑定 task_id、无进度条；仅日志 +
// gallery.changed。
auto schedule_silent_extract_photo_params(
    Core::State::AppState& app_state,
    Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest request) -> void {
  if (!app_state.async) {
    Logger().warn(
        "Silent Infinity Nikki photo params extract skipped: async state is not initialized");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Logger().warn(
        "Silent Infinity Nikki photo params extract skipped: async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, request = std::move(request)]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);

        if (Core::Tasks::has_active_task_of_type(app_state, kExtractPhotoParamsTaskType)) {
          Logger().debug(
              "Silent Infinity Nikki photo params extract skipped: user-initiated extract task is "
              "active");
          co_return;
        }

        auto extract_result =
            co_await Extensions::InfinityNikki::PhotoExtract::extract_photo_params(app_state,
                                                                                   request, {});

        if (!extract_result) {
          Logger().error("Silent Infinity Nikki photo params extract failed: {}",
                         extract_result.error());
          co_return;
        }

        const auto& summary = extract_result.value();
        if (summary.failed_count > 0) {
          auto warning_message = std::format(
              "Silent Infinity Nikki photo params extract completed with warnings: failed {} / "
              "processed {}",
              summary.failed_count, summary.processed_count);
          append_task_error_details(warning_message, summary.errors);
          Logger().warn("{}", warning_message);
        } else {
          Logger().info(
              "Silent Infinity Nikki photo params extract completed: candidates={}, "
              "processed={}, saved={}, skipped={}, failed={}",
              summary.candidate_count, summary.processed_count, summary.saved_count,
              summary.skipped_count, summary.failed_count);
        }

        if (summary.saved_count > 0) {
          Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
        }

        co_return;
      },
      asio::detached);
}

// 全量建立/校正游戏 ScreenShot 与图库侧的硬链接；完成后可能把设置里 manage_screenshot_hardlinks
// 置为 true。
auto launch_initialize_screenshot_hardlinks_task(Core::State::AppState& app_state,
                                                 const std::string& task_id) -> void {
  if (!app_state.async) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async state is not initialized");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Core::Tasks::complete_task_failed(app_state, task_id, "Async runtime is not available");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, task_id]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);
        Core::Tasks::mark_task_running(app_state, task_id);

        auto last_emit_at = std::chrono::steady_clock::time_point{};
        auto last_percent = -1;
        auto progress_callback =
            [&app_state, &task_id, &last_emit_at, &last_percent](
                const Extensions::InfinityNikki::InfinityNikkiInitializeScreenshotHardlinksProgress&
                    progress) {
              auto percent = static_cast<int>(std::floor(progress.percent.value_or(0.0)));
              auto now = std::chrono::steady_clock::now();
              bool should_emit = false;
              if (progress.stage == "completed") {
                should_emit = true;
              } else if (last_emit_at == std::chrono::steady_clock::time_point{}) {
                should_emit = true;
              } else if (percent > last_percent && now - last_emit_at >= kProgressEmitInterval) {
                should_emit = true;
              }

              if (!should_emit) {
                return;
              }

              last_emit_at = now;
              last_percent = std::max(last_percent, percent);
              Core::Tasks::TaskProgress task_progress{
                  .stage = progress.stage,
                  .current = progress.current,
                  .total = progress.total,
                  .percent = progress.percent,
                  .message = progress.message,
              };
              Core::Tasks::update_task_progress(app_state, task_id, task_progress);
            };

        auto initialize_result = Extensions::InfinityNikki::ScreenshotHardlinks::initialize(
            app_state, progress_callback);
        if (!initialize_result) {
          auto error_message =
              "Infinity Nikki screenshot hardlinks initialize failed: " + initialize_result.error();
          Logger().error("{}", error_message);
          Core::Tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }

        const auto& summary = initialize_result.value();
        Core::Tasks::update_task_progress(
            app_state, task_id,
            Core::Tasks::TaskProgress{
                .stage = "completed",
                .current = summary.source_count,
                .total = summary.source_count,
                .percent = 100.0,
                .message =
                    std::format("Source {}, created {}, updated {}, removed {}, ignored {}",
                                summary.source_count, summary.created_count, summary.updated_count,
                                summary.removed_count, summary.ignored_count),
            });

        if (!summary.errors.empty()) {
          auto error_message = make_screenshot_hardlinks_task_error_message(summary);
          Logger().error("{}", error_message);
          Core::Tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }

        if (app_state.settings &&
            !app_state.settings->raw.extensions.infinity_nikki.manage_screenshot_hardlinks) {
          auto next_settings = app_state.settings->raw;
          next_settings.extensions.infinity_nikki.manage_screenshot_hardlinks = true;
          if (auto save_result = Features::Settings::update_settings(app_state, next_settings);
              !save_result) {
            Logger().warn("Failed to persist Infinity Nikki screenshot hardlink setting: {}",
                          save_result.error());
          }
        }

        Core::Tasks::complete_task_success(app_state, task_id);
      },
      asio::detached);
}

// ---------------------------------------------------------------------------
// start_*：对外 API；先防重（同类任务已活跃则直接报错），再 create_task，最后 launch_*。

auto start_initial_scan_task(
    Core::State::AppState& app_state, const Features::Gallery::Types::ScanOptions& options,
    std::function<void(const Features::Gallery::Types::ScanResult&)> post_scan_callback)
    -> std::expected<std::string, std::string> {
  if (Core::Tasks::has_active_task_of_type(app_state, kInitialScanTaskType)) {
    return std::unexpected("Another Infinity Nikki initial scan task is already running");
  }

  auto task_id = Core::Tasks::create_task(app_state, kInitialScanTaskType, options.directory);
  if (task_id.empty()) {
    return std::unexpected("Failed to create Infinity Nikki initial scan task");
  }

  launch_initial_scan_task(app_state, options, task_id, std::move(post_scan_callback));
  return task_id;
}

// 与 schedule_silent_extract_photo_params 互斥体现在：静默路径会查 has_active_task_of_type，
// 此处则禁止两个「带任务的解析」并行；不阻止静默协程与任务解析在理论上重叠（接受偶尔重复解析）。
auto start_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::string, std::string> {
  if (Core::Tasks::has_active_task_of_type(app_state, kExtractPhotoParamsTaskType)) {
    return std::unexpected("Another Infinity Nikki extract task is already running");
  }

  auto task_id = Core::Tasks::create_task(app_state, kExtractPhotoParamsTaskType);
  if (task_id.empty()) {
    return std::unexpected("Failed to create Infinity Nikki extract task");
  }

  launch_extract_photo_params_task(app_state, request, task_id);
  return task_id;
}

// RPC / 图库菜单「提取元数据」入口；业务参数收敛到 InfinityNikkiExtractPhotoParamsRequest。
auto start_extract_photo_params_for_folder_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsForFolderRequest& request)
    -> std::expected<std::string, std::string> {
  if (request.folder_id <= 0) {
    return std::unexpected("Invalid folder id for manual Infinity Nikki extract");
  }

  if (!is_numeric_uid(request.uid)) {
    return std::unexpected("UID must be a non-empty numeric string");
  }

  return start_extract_photo_params_task(
      app_state, Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest{
                     .only_missing = request.only_missing,
                     .folder_id = request.folder_id,
                     .uid_override = request.uid,
                 });
}

auto start_initialize_screenshot_hardlinks_task(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string> {
  if (Core::Tasks::has_active_task_of_type(app_state, kInitializeScreenshotHardlinksTaskType)) {
    return std::unexpected("Another Infinity Nikki screenshot hardlink task is already running");
  }

  auto task_id = Core::Tasks::create_task(app_state, kInitializeScreenshotHardlinksTaskType);
  if (task_id.empty()) {
    return std::unexpected("Failed to create Infinity Nikki screenshot hardlink task");
  }

  launch_initialize_screenshot_hardlinks_task(app_state, task_id);
  return task_id;
}

}  // namespace Extensions::InfinityNikki::TaskService
