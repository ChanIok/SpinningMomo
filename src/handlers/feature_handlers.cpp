module;

module Handlers.Feature;

import std;
import Core.Config.Io;
import Core.Events;
import Core.State;
import UI.AppWindow;
import Utils.Logger;

namespace Handlers {

auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册功能开关事件处理器
  subscribe(app_state.event_bus, EventType::ToggleFeature, [&app_state](const Event& event) {
    auto data = std::any_cast<FeatureToggleData>(event.data);
    Logger().debug("Feature toggled");

    bool config_changed = false;
    switch (data.feature) {
      case FeatureType::Preview:
        UI::AppWindow::set_preview_enabled(app_state, data.enabled);
        // app_state.config.menu.use_floating_window = data.enabled; // 假设预览就是浮动窗口
        // config_changed = true;
        break;
      case FeatureType::Overlay:
        UI::AppWindow::set_overlay_enabled(app_state, data.enabled);
        // Overlay 通常是临时状态，不写入配置
        break;
      case FeatureType::Letterbox:
        UI::AppWindow::set_letterbox_enabled(app_state, data.enabled);
        app_state.config.letterbox.enabled = data.enabled;
        config_changed = true;
        break;
    }

    if (config_changed) {
      if (auto result = Core::Config::Io::save(app_state.config); !result) {
        Logger().error("Failed to save config: {}", result.error());
      }
    }
  });
}

} 