module;

module Handlers.Feature;

import std;
import Core.Actions;
import Core.Config.Io;
import Core.Events;
import Core.State;
import Features.Letterbox;
import Features.Preview.Window;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay;
import Utils.Logger;

namespace Handlers {

// 处理预览功能切换
auto handle_preview_toggle(Core::State::AppState& state, bool enabled) -> void {
  Core::Actions::dispatch_action(
      state, Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = enabled}});

  if (enabled) {
    // 查找目标窗口
    auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
    if (target_window) {
      if (auto result = Features::Preview::Window::start_preview(state, target_window.value());
          !result) {
        Logger().error("Failed to start preview");
        // 回滚UI状态
        Core::Actions::dispatch_action(
            state, Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = false}});
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to start preview window");
      }
    } else {
      Logger().warn("No target window found for preview");
      Core::Actions::dispatch_action(
          state, Core::Actions::Action{Core::Actions::Payloads::TogglePreview{.enabled = false}});
      Features::Notifications::show_notification(
          state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    }
  } else {
    // 停止预览
    Features::Preview::Window::stop_preview(state);
  }
}

// 处理叠加层功能切换
auto handle_overlay_toggle(Core::State::AppState& state, bool enabled) -> void {
  Core::Actions::dispatch_action(
      state, Core::Actions::Action{Core::Actions::Payloads::ToggleOverlay{.enabled = enabled}});

  if (enabled) {
    // 查找目标窗口
    auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
    if (target_window) {
      if (auto result = Features::Overlay::start_overlay(state, target_window.value()); !result) {
        Logger().error("Failed to start overlay: {}", result.error());
        // 回滚UI状态
        Core::Actions::dispatch_action(
            state, Core::Actions::Action{Core::Actions::Payloads::ToggleOverlay{.enabled = false}});
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to start overlay window");
      }
    } else {
      Logger().warn("No target window found for overlay");
      Core::Actions::dispatch_action(
          state, Core::Actions::Action{Core::Actions::Payloads::ToggleOverlay{.enabled = false}});
      Features::Notifications::show_notification(
          state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    }
  } else {
    // 停止叠加层
    Features::Overlay::stop_overlay(state);
  }
}

// 处理letterbox功能切换
auto handle_letterbox_toggle(Core::State::AppState& state, bool enabled) -> void {
  Core::Actions::dispatch_action(
      state, Core::Actions::Action{Core::Actions::Payloads::ToggleLetterbox{.enabled = enabled}});
  state.config.letterbox.enabled = enabled;

  if (enabled) {
    // 查找目标窗口
    auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
    if (target_window) {
      if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
        Logger().error("Failed to show letterbox: {}", result.error());
        // 回滚UI状态
        Core::Actions::dispatch_action(
            state,
            Core::Actions::Action{Core::Actions::Payloads::ToggleLetterbox{.enabled = false}});
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to show letterbox window");
      }
    } else {
      Logger().warn("No target window found for letterbox");
      Core::Actions::dispatch_action(
          state, Core::Actions::Action{Core::Actions::Payloads::ToggleLetterbox{.enabled = false}});
      Features::Notifications::show_notification(
          state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    }
  } else {
    // 隐藏letterbox
    if (auto result = Features::Letterbox::hide(state); !result) {
      Logger().error("Failed to hide letterbox: {}", result.error());
    }
  }

  // 如果叠加层正在运行，更新其黑边模式设置
  if (Features::Overlay::is_overlay_capturing(state)) {
    Features::Overlay::set_letterbox_mode(state, enabled);
  }
}

auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册功能开关事件处理器
  subscribe(app_state.event_bus, EventType::ToggleFeature, [&app_state](const Event& event) {
    auto data = std::any_cast<FeatureToggleData>(event.data);
    Logger().debug("Feature toggled");

    bool config_changed = false;
    switch (data.feature) {
      case FeatureType::Preview:
        handle_preview_toggle(app_state, data.enabled);
        break;
      case FeatureType::Overlay:
        handle_overlay_toggle(app_state, data.enabled);
        break;
      case FeatureType::Letterbox:
        handle_letterbox_toggle(app_state, data.enabled);
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