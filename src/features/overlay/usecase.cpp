module;

module Features.Overlay.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import UI.FloatingWindow;
import Features.Overlay;
import Features.Letterbox;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay.State;
import Utils.Logger;
import Utils.String;

namespace Features::Overlay::UseCase {

// 切换叠加层功能
auto toggle_overlay(Core::State::AppState& state) -> void {
  bool is_running = state.overlay && state.overlay->running;

  // 更新叠加层状态
  UI::FloatingWindow::set_overlay_enabled(state, !is_running);

  if (!is_running) {
    // 启动叠加层
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
        // 回滚 UI状态
        UI::FloatingWindow::set_overlay_enabled(state, false);
        // 使用新的消息定义并附加错误详情
        std::string error_message =
            state.i18n->texts["message.overlay_start_failed"] + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                   error_message);
      }
    } else {
      Logger().warn("No target window found for overlay");
      UI::FloatingWindow::set_overlay_enabled(state, false);
      Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                 state.i18n->texts["message.window_not_found"]);
    }
  } else {
    // 停止叠加层
    Features::Overlay::stop_overlay(state);

    // 如果启用了黑边模式，重新显示黑边窗口
    if (state.settings->raw.features.letterbox.enabled) {
      std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
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