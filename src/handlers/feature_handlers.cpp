module;

module Handlers.Feature;

import std;
import Core.Events;
import Core.State;
import Core.I18n.State;
import UI.AppWindow;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import Features.Settings;
import Features.Settings.State;
import Features.Letterbox;
import Features.Preview.Window;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay;
import Features.Overlay.State;
import Features.Screenshot;
import Features.Screenshot.Folder;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;

namespace Handlers {

// 处理预览功能切换
auto handle_preview_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::PreviewToggleEvent& event) -> void {
  // 更新预览状态
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
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   state.i18n->texts.message.window_adjust_failed);
      }
    } else {
      Logger().warn("No target window found for preview");
      UI::AppWindow::set_preview_enabled(state, false);
      Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                 state.i18n->texts.message.window_not_found);
    }
  } else {
    // 停止预览
    Features::Preview::Window::stop_preview(state);
  }
}

// 处理叠加层功能切换
auto handle_overlay_toggle(Core::State::AppState& state,
                           const UI::AppWindow::Events::OverlayToggleEvent& event) -> void {
  // 更新叠加层状态
  UI::AppWindow::set_overlay_enabled(state, event.enabled);

  if (event.enabled) {
    // 如果启用了黑边模式，关闭黑边窗口
    if (state.settings->config.features.letterbox.enabled) {
      if (auto result = Features::Letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
      }
    }

    std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
    auto target_window = Features::WindowControl::find_target_window(window_title);
    if (target_window) {
      if (auto result = Features::Overlay::start_overlay(state, target_window.value()); !result) {
        Logger().error("Failed to start overlay: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_overlay_enabled(state, false);
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   state.i18n->texts.message.window_adjust_failed);
      }
    } else {
      Logger().warn("No target window found for overlay");
      UI::AppWindow::set_overlay_enabled(state, false);
      Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                 state.i18n->texts.message.window_not_found);
    }
  } else {
    // 停止叠加层
    Features::Overlay::stop_overlay(state);

    // 如果启用了黑边模式，重新显示黑边窗口
    if (state.settings->config.features.letterbox.enabled) {
      std::wstring window_title =
          Utils::String::FromUtf8(state.settings->config.window.target_title);
      auto target_window = Features::WindowControl::find_target_window(window_title);
      if (target_window) {
        if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
          Logger().error("Failed to show letterbox: {}", result.error());
        }
      }
    }
  }
}

// 处理letterbox功能切换
auto handle_letterbox_toggle(Core::State::AppState& state,
                             const UI::AppWindow::Events::LetterboxToggleEvent& event) -> void {
  // 更新黑边状态（使用UI.AppWindow的接口函数）
  UI::AppWindow::set_letterbox_enabled(state, event.enabled);

  // 更新叠加层状态中的letterbox启用状态
  Features::Overlay::set_letterbox_mode(state, event.enabled);

  // 更新设置状态中的letterbox启用状态
  state.settings->config.features.letterbox.enabled = event.enabled;

  // 保存设置到文件
  auto settings_path = Features::Settings::get_settings_path();
  if (settings_path) {
    auto save_result =
        Features::Settings::save_settings_to_file(settings_path.value(), state.settings->config);
    if (!save_result) {
      Logger().error("Failed to save settings: {}", save_result.error());
    }
  } else {
    Logger().error("Failed to get settings path: {}", settings_path.error());
  }

  std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);

  // 如果叠加层正在捕获，重启捕获以应用新设置
  if (state.overlay->running) {
    if (target_window) {
      Features::Overlay::stop_overlay(state);
      auto start_result = Features::Overlay::start_overlay(state, target_window.value());
      // 关闭letterbox
      if (auto result = Features::Letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
      }
      if (!start_result) {
        Logger().error("Failed to restart overlay after letterbox mode change: {}",
                       start_result.error());
        // 回滚UI状态
        UI::AppWindow::set_letterbox_enabled(state, false);
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   state.i18n->texts.message.window_adjust_failed);
      }
    }
    return;
  }

  // 如果启用letterbox，检查当前窗口是否需要显示letterbox
  if (event.enabled) {
    if (target_window) {
      if (auto result = Features::Letterbox::show(state, target_window.value()); !result) {
        Logger().error("Failed to show letterbox: {}", result.error());
        // 回滚UI状态
        UI::AppWindow::set_letterbox_enabled(state, false);
        Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                   state.i18n->texts.message.window_adjust_failed);
      }
    }
  } else {
    // 关闭letterbox
    if (auto result = Features::Letterbox::shutdown(state); !result) {
      Logger().error("Failed to shutdown letterbox: {}", result.error());
    }
  }
}

// 处理截图事件
auto handle_capture_event(Core::State::AppState& state,
                          const UI::AppWindow::Events::CaptureEvent& event) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

  // 创建截图完成回调
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      // 转换路径为字符串用于通知
      std::string path_str(path.begin(), path.end());
      Features::Notifications::show_notification(
          state, state.i18n->texts.label.app_name,
          state.i18n->texts.message.screenshot_success + path_str);
      Logger().debug("Screenshot saved successfully: {}", path_str);
    } else {
      Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                                 state.i18n->texts.message.window_adjust_failed);
      Logger().error("Screenshot capture failed");
    }
  };

  // 执行截图
  auto result = Features::Screenshot::take_screenshot(state, *target_window, completion_callback);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts.label.app_name,
        state.i18n->texts.message.window_adjust_failed + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理打开截图文件夹事件
auto handle_screenshots_event(Core::State::AppState& state,
                              const UI::AppWindow::Events::ScreenshotsEvent& event) -> void {
  Logger().debug("Opening screenshot folder");

  if (auto result = Features::Screenshot::Folder::open_folder(state); !result) {
    Logger().error("Failed to open screenshot folder: {}", result.error());
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_adjust_failed);
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

  subscribe<UI::AppWindow::Events::CaptureEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::CaptureEvent& event) {
        handle_capture_event(app_state, event);
      });

  subscribe<UI::AppWindow::Events::ScreenshotsEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::ScreenshotsEvent& event) {
        handle_screenshots_event(app_state, event);
      });
}

}  // namespace Handlers