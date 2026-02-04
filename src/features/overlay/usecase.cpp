module;

module Features.Overlay.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import Features.Overlay;
import Features.Letterbox;
import Features.Letterbox.State;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay.State;
import Utils.Logger;
import Utils.String;

namespace Features::Overlay::UseCase {

// 切换叠加层功能
auto toggle_overlay(Core::State::AppState& state) -> void {
  bool is_enabled = state.overlay->enabled;

  // 切换启用状态
  state.overlay->enabled = !is_enabled;

  if (!is_enabled) {
    // 用户想启用叠加层
    // 如果启用了黑边模式，关闭黑边窗口
    if (state.letterbox->enabled) {
      if (auto result = Features::Letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
      }
    }

    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);

    if (target_window) {
      if (auto result = Features::Overlay::start_overlay(state, target_window.value()); !result) {
        Logger().error("Failed to start overlay: {}", result.error());
        // 回滚启用状态
        state.overlay->enabled = false;
        // 使用新的消息定义并附加错误详情
        std::string error_message =
            state.i18n->texts["message.overlay_start_failed"] + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                   error_message);
      }
    } else {
      // 找不到目标窗口
      Logger().warn("No target window found for overlay");
      state.overlay->enabled = false;
      Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                 state.i18n->texts["message.window_not_found"]);
    }
  } else {
    // 用户想停用叠加层
    Features::Overlay::stop_overlay(state);

    // 如果启用了黑边模式，重新显示黑边窗口
    if (state.letterbox->enabled) {
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