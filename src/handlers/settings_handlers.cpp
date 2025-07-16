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

    // TODO: 实现具体的设置变更处理逻辑
    // 由于当前的事件数据结构是空的，我们暂时跳过详细的变更检查
    // 未来可以通过比较新旧设置来实现具体的变更处理
    
    // 可以在这里添加具体的设置变更处理逻辑
    // 例如：
    // - 更新窗口标题
    // - 通知其他模块菜单配置已变更
    // - 刷新UI等

    Logger().debug("Settings change processing completed");

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