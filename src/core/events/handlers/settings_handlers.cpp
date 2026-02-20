module;

module Core.Events.Handlers.Settings;

import std;
import Core.Events;
import Core.RPC.NotificationHub;
import Core.State;
import Core.Commands;
import Features.Settings.Events;
import Features.Settings.Types;
import UI.FloatingWindow;
import UI.FloatingWindow.State;
import Utils.Logger;

namespace Core::Events::Handlers {

auto has_hotkey_changes(const Features::Settings::Types::AppSettings& old_settings,
                        const Features::Settings::Types::AppSettings& new_settings) -> bool {
  const auto& old_hotkey = old_settings.app.hotkey;
  const auto& new_hotkey = new_settings.app.hotkey;

  return old_hotkey.floating_window.modifiers != new_hotkey.floating_window.modifiers ||
         old_hotkey.floating_window.key != new_hotkey.floating_window.key ||
         old_hotkey.screenshot.modifiers != new_hotkey.screenshot.modifiers ||
         old_hotkey.screenshot.key != new_hotkey.screenshot.key ||
         old_hotkey.recording.modifiers != new_hotkey.recording.modifiers ||
         old_hotkey.recording.key != new_hotkey.recording.key;
}

auto refresh_global_hotkeys(Core::State::AppState& state) -> void {
  if (!state.floating_window || !state.floating_window->window.hwnd) {
    Logger().warn("Skip hotkey refresh: floating window handle is not ready");
    return;
  }

  auto hwnd = state.floating_window->window.hwnd;
  Core::Commands::unregister_all_hotkeys(state, hwnd);
  Core::Commands::register_all_hotkeys(state, hwnd);
  Logger().info("Global hotkeys refreshed from latest settings");
}

// 处理设置变更事件
auto handle_settings_changed(Core::State::AppState& state,
                             const Features::Settings::Events::SettingsChangeEvent& event) -> void {
  try {
    Logger().info("Settings changed: {}", event.data.change_description);

    // 通知浮窗刷新UI以反映设置变更
    UI::FloatingWindow::refresh_from_settings(state);

    if (has_hotkey_changes(event.data.old_settings, event.data.new_settings)) {
      refresh_global_hotkeys(state);
    }

    Core::RPC::NotificationHub::send_notification(state, "settings.changed");

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
