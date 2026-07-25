#include "features/screenshot/usecase.hpp"

#include "vendor/std.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "features/photography/state.hpp"
#include "features/screenshot/screenshot.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/floating_window/events.hpp"
#include "utils/image/image.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"
#include "utils/system/system.hpp"

namespace features::screenshot {

auto handle_saved_file_view_action(core::AppState& state, const std::filesystem::path& path,
                                   std::string_view file_kind) -> void {
  const auto& action = state.settings->raw.features.saved_file_view_action;
  auto action_result = action == "reveal_in_explorer"
                           ? utils::system::reveal_file_in_explorer(path)
                           : utils::system::open_file_with_default_app(path);
  if (!action_result) {
    Logger().warn("Failed to handle {} view action '{}': {}", file_kind, action,
                  action_result.error());
  }
}

// 截图
auto capture(core::AppState& state) -> void {
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = features::window_control::find_target_window(window_title);
  if (!target_window) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    return;
  }

  std::optional<std::filesystem::path> output_dir_override;
  if (state.settings->raw.features.organize_output_by_window_title) {
    auto actual_title = features::window_control::get_window_title(*target_window);
    if (!actual_title) {
      Logger().warn("Failed to read current screenshot target title, using configured title: {}",
                    actual_title.error());
    }

    const auto title = actual_title.value_or(window_title);
    auto output_dir_result = utils::path::GetOutputDirectoryForWindowTitle(
        state.settings->raw.features.output_dir_path, title);
    if (!output_dir_result) {
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.screenshot_failed"] + ": " + output_dir_result.error());
      Logger().error("Failed to resolve screenshot output directory: {}",
                     output_dir_result.error());
      return;
    }
    output_dir_override = *output_dir_result;
  }

  // 截图完成回调在截图工作线程的帧回调中执行，必须快速返回；通知通过事件系统发送到 UI 线程
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      const std::filesystem::path screenshot_path(path);
      const auto path_str = utils::string::ToUtf8(path);

      core::notifications::NotificationOptions options;
      options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
      options.message =
          utils::string::FromUtf8(state.i18n->texts["message.screenshot_success"]) + path;

      core::notifications::NotificationAction view_action;
      view_action.label = utils::string::FromUtf8(state.i18n->texts["notification.action.view"]);
      view_action.callback = [screenshot_path](core::AppState& app_state) {
        handle_saved_file_view_action(app_state, screenshot_path, "screenshot");
      };
      options.action = std::move(view_action);

      core::notifications::post_notification_request(state, std::move(options));
      Logger().info("Screenshot saved successfully: {}", path_str);
    } else {
      core::notifications::NotificationOptions fail_options;
      fail_options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
      fail_options.message =
          utils::string::FromUtf8(state.i18n->texts["message.screenshot_failed"]);
      core::notifications::post_notification_request(state, std::move(fail_options));
      Logger().error("Screenshot capture failed");
    }
  };

  utils::image::ImageFormat image_format = utils::image::ImageFormat::PNG;
  const auto& fmt = state.settings->raw.features.screenshot.file_format;
  if (fmt == "jpeg" || fmt == "jpg") {
    image_format = utils::image::ImageFormat::JPEG;
  }
  float jpeg_quality = 1.0f;

  // 若高级摄影模式开启，将帧数传入截图管道以启用长曝光累积
  int shutter_frames = 0;
  if (state.photography->enabled.load(std::memory_order_acquire)) {
    shutter_frames = std::max(0, state.photography->shutter_frames.load(std::memory_order_acquire));
  }

  const auto capture_client_area = state.settings->raw.features.screenshot.capture_client_area;

  auto result = features::screenshot::take_screenshot(
      state, *target_window, std::move(completion_callback), image_format, jpeg_quality,
      output_dir_override, shutter_frames, capture_client_area);
  if (!result) {
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.screenshot_failed"] + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理截图事件（Event版本，用于热键系统）
auto handle_capture_event(core::AppState& state,
                          const ui::floating_window::events::CaptureEvent& event) -> void {
  static_cast<void>(event);
  capture(state);
}

}  // namespace features::screenshot
