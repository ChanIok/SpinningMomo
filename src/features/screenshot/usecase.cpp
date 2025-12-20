module;

module Features.Screenshot.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import UI.AppWindow.Events;
import Features.Screenshot;
import Features.Screenshot.Folder;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
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

  // 创建截图完成回调
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      // 转换路径为字符串用于通知
      std::string path_str(path.begin(), path.end());
      Features::Notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.screenshot_success"] + path_str);
      Logger().debug("Screenshot saved successfully: {}", path_str);
    } else {
      Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                 state.i18n->texts["message.window_adjust_failed"]);
      Logger().error("Screenshot capture failed");
    }
  };

  // 执行截图
  auto result = Features::Screenshot::take_screenshot(state, *target_window, completion_callback);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理截图事件（Event版本，用于热键系统）
auto handle_capture_event(Core::State::AppState& state,
                          const UI::AppWindow::Events::CaptureEvent& event) -> void {
  capture(state);
}

}  // namespace Features::Screenshot::UseCase