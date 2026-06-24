module;

module Features.Screenshot.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import Core.Notifications;
import Core.Notifications.Types;
import UI.FloatingWindow.Events;
import Features.Screenshot;
import Features.Photography.State;
import Features.Settings.State;
import Features.WindowControl;
import Utils.Image;
import Utils.Logger;
import Utils.String;
import Utils.System;

namespace Features::Screenshot::UseCase {

auto handle_saved_file_view_action(Core::State::AppState& state, const std::filesystem::path& path,
                                   std::string_view file_kind) -> void {
  const auto& action = state.settings->raw.features.saved_file_view_action;
  auto action_result = action == "reveal_in_explorer"
                           ? Utils::System::reveal_file_in_explorer(path)
                           : Utils::System::open_file_with_default_app(path);
  if (!action_result) {
    Logger().warn("Failed to handle {} view action '{}': {}", file_kind, action,
                  action_result.error());
  }
}

// 截图
auto capture(Core::State::AppState& state) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Core::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 截图完成回调在截图工作线程的帧回调中执行，必须快速返回；通知通过事件系统发送到 UI 线程
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      const std::filesystem::path screenshot_path(path);
      const auto path_str = Utils::String::ToUtf8(path);

      Core::Notifications::Types::NotificationOptions options;
      options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
      options.message =
          Utils::String::FromUtf8(state.i18n->texts["message.screenshot_success"]) + path;

      Core::Notifications::Types::NotificationAction view_action;
      view_action.label = Utils::String::FromUtf8(state.i18n->texts["notification.action.view"]);
      view_action.callback = [screenshot_path](Core::State::AppState& app_state) {
        handle_saved_file_view_action(app_state, screenshot_path, "screenshot");
      };
      options.action = std::move(view_action);

      Core::Notifications::post_notification_request(state, std::move(options));
      Logger().info("Screenshot saved successfully: {}", path_str);
    } else {
      Core::Notifications::Types::NotificationOptions fail_options;
      fail_options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
      fail_options.message =
          Utils::String::FromUtf8(state.i18n->texts["message.screenshot_failed"]);
      Core::Notifications::post_notification_request(state, std::move(fail_options));
      Logger().error("Screenshot capture failed");
    }
  };

  Utils::Image::ImageFormat image_format = Utils::Image::ImageFormat::PNG;
  const auto& fmt = state.settings->raw.features.screenshot.file_format;
  if (fmt == "jpeg" || fmt == "jpg") {
    image_format = Utils::Image::ImageFormat::JPEG;
  }
  float jpeg_quality = 1.0f;

  // 若高级摄影模式开启，将帧数传入截图管道以启用长曝光累积
  int shutter_frames = 0;
  if (state.photography->enabled.load(std::memory_order_acquire)) {
    shutter_frames = std::max(0, state.photography->shutter_frames.load(std::memory_order_acquire));
  }

  auto result = Features::Screenshot::take_screenshot(state, *target_window, completion_callback,
                                                      image_format, jpeg_quality, std::nullopt,
                                                      shutter_frames);
  if (!result) {
    Core::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.screenshot_failed"] + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理截图事件（Event版本，用于热键系统）
auto handle_capture_event(Core::State::AppState& state,
                          const UI::FloatingWindow::Events::CaptureEvent& event) -> void {
  static_cast<void>(event);
  capture(state);
}

}  // namespace Features::Screenshot::UseCase
