module;

module Features.Preview.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import Features.Preview;
import Features.Overlay;
import Features.Overlay.State;
import Features.Letterbox;
import Features.Letterbox.State;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Preview.State;
import Utils.Logger;
import Utils.String;

namespace Features::Preview::UseCase {

// 切换预览功能
auto toggle_preview(Core::State::AppState& state) -> void {
  bool is_running = state.preview && state.preview->running;

  if (!is_running) {
    // 启动预览
    // 预览窗与叠加层互斥，若叠加层运行则先关闭
    if (state.overlay->running) {
      state.overlay->enabled = false;
      Features::Overlay::stop_overlay(state);
      if (state.letterbox->enabled) {
        std::wstring lb_window_title =
            Utils::String::FromUtf8(state.settings->raw.window.target_title);
        auto lb_target_window = Features::WindowControl::find_target_window(lb_window_title);
        if (lb_target_window) {
          if (auto lb_result = Features::Letterbox::show(state, lb_target_window.value());
              !lb_result) {
            Logger().error("Failed to show letterbox: {}", lb_result.error());
          }
        }
      }
      Features::Notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.preview_overlay_conflict"]);
    }

    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);

    if (target_window) {
      if (auto result = Features::Preview::start_preview(state, target_window.value()); !result) {
        Logger().error("Failed to start preview: {}", result.error());
        // 使用新的消息定义并附加错误详情
        std::string error_message =
            state.i18n->texts["message.preview_start_failed"] + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                   error_message);
      }
    } else {
      Logger().warn("No target window found for preview");
      Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                 state.i18n->texts["message.window_not_found"]);
    }
  } else {
    // 停止预览
    Features::Preview::stop_preview(state);
  }
}

}  // namespace Features::Preview::UseCase