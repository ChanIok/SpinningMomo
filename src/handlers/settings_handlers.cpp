module;

module Handlers.Settings;

import std;
import Core.Events;
import Core.State;
import Core.Settings.Events;
import Features.Settings.Types;
import UI.AppWindow;
import Utils.Logger;

namespace Handlers {

// 处理设置变更事件
auto handle_settings_changed(Core::State::AppState& state,
                             const Core::Settings::Events::SettingsChangeEvent& event) -> void {
  try {
    Logger().info("Settings changed: {}", event.data.change_description);

    // 通知app_window刷新UI以反映设置变更
    UI::AppWindow::refresh_from_settings(state);

    Logger().debug("Settings change processing completed");

  } catch (const std::exception& e) {
    Logger().error("Error handling settings change event: {}", e.what());
  }
}

auto register_settings_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册设置变更事件处理器
  subscribe<Core::Settings::Events::SettingsChangeEvent>(
      *app_state.event_bus, [&app_state](const Core::Settings::Events::SettingsChangeEvent& event) {
        handle_settings_changed(app_state, event);
      });

  Logger().info("Settings handlers registered successfully");
}

}  // namespace Handlers