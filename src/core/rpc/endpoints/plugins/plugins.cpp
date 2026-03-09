module;

#include <asio.hpp>

module Core.RPC.Endpoints.PluginEndpoints;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.NotificationHub;
import Core.Async;
import Core.Tasks;
import Features.Settings;
import Features.Settings.State;
import Plugins.InfinityNikki.Types;
import Plugins.InfinityNikki.GameDirectory;
import Plugins.InfinityNikki.PhotoExtract;
import Plugins.InfinityNikki.ScreenshotShortcuts;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::PluginEndpoints {

struct StartPluginTaskResult {
  std::string task_id;
};

auto launch_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Plugins::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request,
    const std::string& task_id) -> void {
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
            [&app_state, &task_id](
                const Plugins::InfinityNikki::InfinityNikkiExtractPhotoParamsProgress& progress) {
              Core::Tasks::TaskProgress task_progress{
                  .stage = progress.stage,
                  .current = progress.current,
                  .total = progress.total,
                  .percent = progress.percent,
                  .message = progress.message,
              };
              Core::Tasks::update_task_progress(app_state, task_id, task_progress);
            };

        auto extract_result = Plugins::InfinityNikki::PhotoExtract::extract_photo_params(
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

auto launch_initialize_screenshot_shortcuts_task(Core::State::AppState& app_state,
                                                 const std::string& task_id) -> void {
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

        auto progress_callback =
            [&app_state, &task_id](
                const Plugins::InfinityNikki::InfinityNikkiInitializeScreenshotShortcutsProgress&
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

        auto initialize_result =
            Plugins::InfinityNikki::ScreenshotShortcuts::initialize(app_state, progress_callback);
        if (!initialize_result) {
          auto error_message =
              "Infinity Nikki screenshot shortcuts initialize failed: " + initialize_result.error();
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
            !app_state.settings->raw.plugins.infinity_nikki.manage_screenshot_shortcuts) {
          auto next_settings = app_state.settings->raw;
          next_settings.plugins.infinity_nikki.manage_screenshot_shortcuts = true;
          if (auto save_result = Features::Settings::update_settings(app_state, next_settings);
              !save_result) {
            Logger().warn("Failed to persist Infinity Nikki screenshot shortcut setting: {}",
                          save_result.error());
          }
        }

        Core::Tasks::complete_task_success(app_state, task_id);
      },
      asio::detached);
}

auto handle_infinity_nikki_get_game_directory([[maybe_unused]] Core::State::AppState& app_state,
                                              [[maybe_unused]] const rfl::Generic& params)
    -> Core::RPC::RpcAwaitable<Plugins::InfinityNikki::InfinityNikkiGameDirResult> {
  auto result = Plugins::InfinityNikki::GameDirectory::get_game_directory();
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Failed to get Infinity Nikki game directory: " + result.error(),
    });
  }

  co_return result.value();
}

auto handle_infinity_nikki_start_extract_photo_params(
    Core::State::AppState& app_state,
    const Plugins::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& params)
    -> Core::RPC::RpcAwaitable<StartPluginTaskResult> {
  constexpr auto kTaskType = "plugins.infinityNikki.extractPhotoParams";
  if (Core::Tasks::has_active_task_of_type(app_state, kTaskType)) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::InvalidRequest),
        .message = "Another Infinity Nikki extract task is already running",
    });
  }

  auto task_id = Core::Tasks::create_task(app_state, kTaskType, "Infinity Nikki photo params");
  if (task_id.empty()) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Failed to create Infinity Nikki extract task",
    });
  }

  launch_extract_photo_params_task(app_state, params, task_id);
  co_return StartPluginTaskResult{.task_id = task_id};
}

auto handle_infinity_nikki_start_initialize_screenshot_shortcuts(
    Core::State::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> Core::RPC::RpcAwaitable<StartPluginTaskResult> {
  constexpr auto kTaskType = "plugins.infinityNikki.initializeScreenshotShortcuts";
  if (Core::Tasks::has_active_task_of_type(app_state, kTaskType)) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::InvalidRequest),
        .message = "Another Infinity Nikki screenshot shortcut task is already running",
    });
  }

  auto task_id =
      Core::Tasks::create_task(app_state, kTaskType, "Infinity Nikki screenshot shortcuts");
  if (task_id.empty()) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Failed to create Infinity Nikki screenshot shortcut task",
    });
  }

  launch_initialize_screenshot_shortcuts_task(app_state, task_id);
  co_return StartPluginTaskResult{.task_id = task_id};
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<rfl::Generic, Plugins::InfinityNikki::InfinityNikkiGameDirResult>(
      app_state, app_state.rpc->registry, "plugins.infinityNikki.getGameDirectory",
      handle_infinity_nikki_get_game_directory,
      "Get Infinity Nikki game installation directory from launcher config");

  Core::RPC::register_method<Plugins::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest,
                             StartPluginTaskResult>(
      app_state, app_state.rpc->registry, "plugins.infinityNikki.startExtractPhotoParams",
      handle_infinity_nikki_start_extract_photo_params,
      "Create a background task to extract and index Infinity Nikki photo params");

  Core::RPC::register_method<rfl::Generic, StartPluginTaskResult>(
      app_state, app_state.rpc->registry,
      "plugins.infinityNikki.startInitializeScreenshotShortcuts",
      handle_infinity_nikki_start_initialize_screenshot_shortcuts,
      "Create a background task to initialize Infinity Nikki ScreenShot shortcuts");

  Logger().info("Plugins RPC endpoints registered");
}

}  // namespace Core::RPC::Endpoints::PluginEndpoints
