module;

module Handlers.Feature;

import std;
import Core.Actions;
import Core.Config.Io;
import Core.Events;
import Core.State;
import Features.Preview.Window;
import Features.WindowControl;
import Features.Notifications;
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
        Core::Actions::dispatch_action(
            app_state,
            Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = data.enabled}});

        // 在action调度后执行实际功能
        if (data.enabled) {
          // 查找目标窗口
          auto target_window =
              Features::WindowControl::find_target_window(app_state.config.window.title);
          if (target_window) {
            if (auto result =
                    Features::Preview::Window::start_preview(app_state, target_window.value());
                !result) {
              Logger().error("Failed to start preview");
              // 回滚UI状态
              Core::Actions::dispatch_action(
                  app_state,
                  Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = false}});
              Features::Notifications::show_notification(app_state, "SpinningMomo",
                                                         "Failed to start preview window");
            }
          } else {
            Logger().warn("No target window found for preview");
            Core::Actions::dispatch_action(
                app_state,
                Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = false}});
            Features::Notifications::show_notification(
                app_state, "SpinningMomo",
                "Target window not found. Please ensure the game is running.");
          }
        } else {
          // 停止预览
          Features::Preview::Window::stop_preview(app_state);
        }
        break;
      case FeatureType::Overlay:
        Core::Actions::dispatch_action(
            app_state,
            Core::Actions::Action{Core::Actions::Payloads::ToggleOverlay{.enabled = data.enabled}});
        // Overlay 通常是临时状态，不写入配置
        break;
      case FeatureType::Letterbox:
        Core::Actions::dispatch_action(
            app_state, Core::Actions::Action{
                           Core::Actions::Payloads::ToggleLetterbox{.enabled = data.enabled}});
        app_state.config.letterbox.enabled = data.enabled;
        config_changed = true;
        break;
    }

    Core::Actions::trigger_ui_update(app_state);

    if (config_changed) {
      if (auto result = Core::Config::Io::save(app_state.config); !result) {
        Logger().error("Failed to save config: {}", result.error());
      }
    }
  });
}

}  // namespace Handlers