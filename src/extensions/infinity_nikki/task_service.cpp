module;

#include <asio.hpp>

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

constexpr auto kInitialScanTaskType = "extensions.infinityNikki.initialScan";
constexpr auto kExtractPhotoParamsTaskType = "extensions.infinityNikki.extractPhotoParams";
constexpr auto kInitializeScreenshotHardlinksTaskType =
    "extensions.infinityNikki.initializeScreenshotHardlinks";
constexpr auto kProgressEmitInterval = std::chrono::milliseconds(250);

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
        Core::Tasks::complete_task_success(app_state, task_id);

        if (summary.saved_count > 0) {
          Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
        }
      },
      asio::detached);
}

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

auto start_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::string, std::string> {
  if (Core::Tasks::has_active_task_of_type(app_state, kExtractPhotoParamsTaskType)) {
    return std::unexpected("Another Infinity Nikki extract task is already running");
  }

  auto task_id = Core::Tasks::create_task(app_state, kExtractPhotoParamsTaskType,
                                          "Infinity Nikki photo params");
  if (task_id.empty()) {
    return std::unexpected("Failed to create Infinity Nikki extract task");
  }

  launch_extract_photo_params_task(app_state, request, task_id);
  return task_id;
}

auto start_initialize_screenshot_hardlinks_task(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string> {
  if (Core::Tasks::has_active_task_of_type(app_state, kInitializeScreenshotHardlinksTaskType)) {
    return std::unexpected("Another Infinity Nikki screenshot hardlink task is already running");
  }

  auto task_id = Core::Tasks::create_task(app_state, kInitializeScreenshotHardlinksTaskType,
                                          "Infinity Nikki screenshot hardlinks");
  if (task_id.empty()) {
    return std::unexpected("Failed to create Infinity Nikki screenshot hardlink task");
  }

  launch_initialize_screenshot_hardlinks_task(app_state, task_id);
  return task_id;
}

}  // namespace Extensions::InfinityNikki::TaskService
