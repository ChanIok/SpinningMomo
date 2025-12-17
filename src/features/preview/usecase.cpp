module;

module Features.Preview.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import Features.Preview;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Logger;
import Utils.String;

namespace Features::Preview::UseCase {

// 处理预览功能切换
auto handle_preview_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::PreviewToggleEvent& event) -> void {
  // 更新预览状态
  UI::AppWindow::set_preview_enabled(state, event.enabled);

  if (event.enabled) {
    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Preview::start_preview(state, target_window.value());
          !result) {
        Logger().error("Failed to start preview: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_preview_enabled(state, false);
        // 使用新的消息定义并附加错误详情
        std::string error_message = state.i18n->texts.message.preview_start_failed + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   error_message);
      }
    } else {
      Logger().warn("No target window found for preview");
      UI::AppWindow::set_preview_enabled(state, false);
      Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                 state.i18n->texts.message.window_not_found);
    }
  } else {
    // 停止预览
    Features::Preview::stop_preview(state);
  }
}

}  // namespace Features::Preview::UseCase