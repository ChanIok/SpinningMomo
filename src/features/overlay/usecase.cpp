module;

module Features.Overlay.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import Features.Overlay;
import Features.Letterbox;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Logger;
import Utils.String;

namespace Features::Overlay::UseCase {

// 处理叠加层功能切换
auto handle_overlay_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::OverlayToggleEvent& event) -> void {
  // 更新叠加层状态
  UI::AppWindow::set_overlay_enabled(state, event.enabled);

  if (event.enabled) {
    // 如果启用了黑边模式，关闭黑边窗口
    if (state.settings->raw.features.letterbox.enabled) {
      if (auto result = Features::Letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
      }
    }

    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Overlay::start_overlay(state, target_window.value()); !result) {
        Logger().error("Failed to start overlay: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_overlay_enabled(state, false);
        // 使用新的消息定义并附加错误详情
        std::string error_message = state.i18n->texts.message.overlay_start_failed + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   error_message);
      }
    } else {
      Logger().warn("No target window found for overlay");
      UI::AppWindow::set_overlay_enabled(state, false);
      Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                 state.i18n->texts.message.window_not_found);
    }
  } else {
    // 停止叠加层
    Features::Overlay::stop_overlay(state);

    // 如果启用了黑边模式，重新显示黑边窗口
    if (state.settings->raw.features.letterbox.enabled) {
      std::wstring window_title =
          Utils::String::FromUtf8(state.settings->raw.window.target_title);
      auto target_window = Features::WindowControl::find_target_window(window_title);
      if (target_window) {
        if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
          Logger().error("Failed to show letterbox: {}", result.error());
        }
      }
    }
  }
}

}  // namespace Features::Overlay::UseCase