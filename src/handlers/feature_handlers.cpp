module;

module Handlers.Feature;

import std;
import Core.Events;
import Core.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import Features.Settings.State;
import Features.Letterbox;
import Features.Preview.Window;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;

namespace Handlers {

// 处理预览功能切换
auto handle_preview_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::PreviewToggleEvent& event) -> void {
  // 更新预览状态（使用UI.AppWindow的接口函数）
  UI::AppWindow::set_preview_enabled(state, event.enabled);

  if (event.enabled) {
    std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Preview::Window::start_preview(state, target_window.value());
          !result) {
        Logger().error("Failed to start preview");
        // 回滚UI状态
        UI::AppWindow::set_preview_enabled(state, false);
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to start preview window");
      }
    } else {
      Logger().warn("No target window found for preview");
      UI::AppWindow::set_preview_enabled(state, false);
      Features::Notifications::show_notification(
          state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    }
  } else {
    // 停止预览
    Features::Preview::Window::stop_preview(state);
  }
}

// 处理叠加层功能切换
auto handle_overlay_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::OverlayToggleEvent& event) -> void {
  // 更新叠加层状态（使用UI.AppWindow的接口函数）
  UI::AppWindow::set_overlay_enabled(state, event.enabled);

  if (event.enabled) {
    std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Overlay::start_overlay(state, target_window.value()); !result) {
        Logger().error("Failed to start overlay: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_overlay_enabled(state, false);
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to start overlay window");
      }
    } else {
      Logger().warn("No target window found for overlay");
      UI::AppWindow::set_overlay_enabled(state, false);
      Features::Notifications::show_notification(
          state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    }
  } else {
    // 停止叠加层
    Features::Overlay::stop_overlay(state);
  }
}

// 处理letterbox功能切换
auto handle_letterbox_toggle(Core::State::AppState& state,
                             const UI::AppWindow::Events::LetterboxToggleEvent& event) -> void {
  // 更新黑边状态（使用UI.AppWindow的接口函数）
  UI::AppWindow::set_letterbox_enabled(state, event.enabled);
  // TODO: 等待settings设计完成后，将letterbox enabled状态保存到settings
  // state.config.letterbox.enabled = enabled; // 暂时注释掉

  if (event.enabled) {
    std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
        Logger().error("Failed to show letterbox: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_letterbox_enabled(state, false);
        Features::Notifications::show_notification(state, "SpinningMomo",
                                                   "Failed to show letterbox window");
      }
    } else {
      Logger().warn("No target window found for letterbox");
      UI::AppWindow::set_letterbox_enabled(state, false);
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
    Features::Overlay::set_letterbox_mode(state, event.enabled);
  }
}

// 注册功能开关事件处理器
auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  subscribe<UI::AppWindow::Events::PreviewToggleEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::PreviewToggleEvent& event) {
        Logger().debug("Preview toggle event received: enabled={}", event.enabled);
        handle_preview_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::OverlayToggleEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::OverlayToggleEvent& event) {
        Logger().debug("Overlay toggle event received: enabled={}", event.enabled);
        handle_overlay_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::LetterboxToggleEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::LetterboxToggleEvent& event) {
        Logger().debug("Letterbox toggle event received: enabled={}", event.enabled);
        handle_letterbox_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });
}

}  // namespace Handlers