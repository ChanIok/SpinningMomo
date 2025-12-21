module;

module Core.Events.Handlers.Settings;

import std;
import Core.Events;
import Core.State;
import Features.Settings.Events;
import Features.Settings.Types;
import UI.FloatingWindow;
import Utils.Logger;

namespace Core::Events::Handlers {

// 处理设置变更事件
auto handle_settings_changed(Core::State::AppState& state,
                             const Features::Settings::Events::SettingsChangeEvent& event) -> void {
  try {
    Logger().info("Settings changed: {}", event.data.change_description);

    // 通知app_window刷新UI以反映设置变更
    UI::FloatingWindow::refresh_from_settings(state);

    Logger().debug("Settings change processing completed");

  } catch (const std::exception& e) {
    Logger().error("Error handling settings change event: {}", e.what());
  }
}

auto register_settings_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册设置变更事件处理器
  subscribe<Features::Settings::Events::SettingsChangeEvent>(
      *app_state.events,
      [&app_state](const Features::Settings::Events::SettingsChangeEvent& event) {
        handle_settings_changed(app_state, event);
      });
}

}  // namespace Core::Events::Handlers