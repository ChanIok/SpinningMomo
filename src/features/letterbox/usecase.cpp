module;

module Features.Letterbox.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import Features.Letterbox;
import Features.Letterbox.State;
import Features.Overlay;
import Features.Overlay.State;
import Features.Settings;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Logger;
import Utils.String;

namespace Features::Letterbox::UseCase {

// 切换黑边模式
auto toggle_letterbox(Core::State::AppState& state) -> void {
  bool is_enabled = state.letterbox->enabled;

  // 切换启用状态
  state.letterbox->enabled = !is_enabled;
  Features::Overlay::set_letterbox_mode(state, !is_enabled);

  // 同步到 settings 并持久化
  state.settings->raw.features.letterbox.enabled = !is_enabled;

  // 保存设置到文件
  auto settings_path = Features::Settings::get_settings_path();
  if (settings_path) {
    auto save_result =
        Features::Settings::save_settings_to_file(settings_path.value(), state.settings->raw);
    if (!save_result) {
      Logger().error("Failed to save settings: {}", save_result.error());
    }
  } else {
    Logger().error("Failed to get settings path: {}", settings_path.error());
  }

  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);

  // 根据叠加层是否运行采取不同的处理方式
  if (state.overlay->running) {
    // 叠加层正在运行时，黑边模式由叠加层模块处理
    // 只需重启叠加层以应用新的黑边模式设置
    if (target_window) {
      Features::Overlay::stop_overlay(state);
      auto start_result = Features::Overlay::start_overlay(state, target_window.value());

      if (!start_result) {
        Logger().error("Failed to restart overlay after letterbox mode change: {}",
                       start_result.error());
        // 回滚启用状态
        state.letterbox->enabled = is_enabled;
        state.settings->raw.features.letterbox.enabled = is_enabled;
        std::string error_message =
            state.i18n->texts["message.overlay_start_failed"] + start_result.error();
        Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                   error_message);
      }
    }
  } else {
    // 叠加层未运行时，黑边模式由letterbox模块处理
    if (!is_enabled) {
      // 启用黑边模式
      if (target_window) {
        if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
          Logger().error("Failed to show letterbox: {}", result.error());
          // 回滚启用状态
          state.letterbox->enabled = false;
          state.settings->raw.features.letterbox.enabled = false;
          std::string error_message =
              state.i18n->texts["message.overlay_start_failed"] + result.error();
          Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                     error_message);
        }
      }
    } else {
      // 禁用黑边模式
      if (auto result = Features::Letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
        std::string error_message =
            state.i18n->texts["message.overlay_start_failed"] + result.error();
        Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                   error_message);
      }
    }
  }
}

}  // namespace Features::Letterbox::UseCase