module;

module Features.Preview.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import Features.Preview;
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