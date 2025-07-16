module;

module Handlers.Settings;

import std;
import Core.Events;
import Core.State;
import Features.Settings.Types;
import Utils.Logger;

namespace Handlers {

// 处理设置变更事件
auto handle_settings_changed(Core::State::AppState& state, const Core::Events::Event& event)
    -> void {
  try {
    auto change_data = std::any_cast<Features::Settings::Types::SettingsChangeData>(event.data);

    Logger().info("Settings changed: {}", change_data.change_description);

    // 记录具体变更内容
    if (change_data.old_settings.window.title != change_data.new_settings.window.title) {
      Logger().info("Window title changed: '{}' -> '{}'", change_data.old_settings.window.title,
                    change_data.new_settings.window.title);
    }

    if (change_data.old_settings.version != change_data.new_settings.version) {
      Logger().info("Version changed: '{}' -> '{}'", change_data.old_settings.version,
                    change_data.new_settings.version);
    }

    // TODO: 在这里添加具体的设置变更处理逻辑
    // 例如：
    // - 更新窗口标题
    // - 通知其他模块
    // - 刷新UI等

  } catch (const std::exception& e) {
    Logger().error("Error handling settings change event: {}", e.what());
  }
}

auto register_settings_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册设置变更事件处理器
  subscribe(app_state.event_bus, EventType::ConfigChanged,
            [&app_state](const Event& event) { handle_settings_changed(app_state, event); });

  Logger().info("Settings handlers registered successfully");
}

}  // namespace Handlers