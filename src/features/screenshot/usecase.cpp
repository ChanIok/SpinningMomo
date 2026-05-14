module;

module Features.Screenshot.UseCase;

import std;
import Core.State;
import Core.Events;
import Core.I18n.State;
import UI.FloatingWindow.Events;
import Features.Screenshot;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Image;
import Utils.Logger;
import Utils.String;

namespace Features::Screenshot::UseCase {

// 截图
auto capture(Core::State::AppState& state) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 截图完成回调在截图工作线程的帧回调中执行，必须快速返回；通知通过事件系统发送到 UI 线程
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      std::string path_str(path.begin(), path.end());

      Core::Events::post(
          *state.events,
          UI::FloatingWindow::Events::NotificationEvent{
              .title = state.i18n->texts["label.app_name"],
              .message = state.i18n->texts["message.screenshot_success"] + path_str});
      Logger().debug("Screenshot saved successfully: {}", path_str);
    } else {
      Core::Events::post(*state.events,
                         UI::FloatingWindow::Events::NotificationEvent{
                             .title = state.i18n->texts["label.app_name"],
                             .message = state.i18n->texts["message.screenshot_failed"]});
      Logger().error("Screenshot capture failed");
    }
  };

  Utils::Image::ImageFormat image_format = Utils::Image::ImageFormat::PNG;
  const auto& fmt = state.settings->raw.features.screenshot.file_format;
  if (fmt == "jpeg" || fmt == "jpg") {
    image_format = Utils::Image::ImageFormat::JPEG;
  }
  float jpeg_quality = 1.0f;

  auto result = Features::Screenshot::take_screenshot(state, *target_window, completion_callback,
                                                      image_format, jpeg_quality, std::nullopt);
  if (!result) {
    Features::Notifications::show_notification(
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
