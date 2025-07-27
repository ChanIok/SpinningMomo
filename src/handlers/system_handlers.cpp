module;

module Handlers.System;

import std;
import Core.Events;
import Core.State;
import Core.WebView;
import Core.WebView.Events;
import UI.AppWindow;
import UI.AppWindow.Layout;
import UI.AppWindow.D2DContext;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import UI.WebViewWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Handlers {

// 从 app_state.ixx 迁移的 DPI 更新函数
auto update_render_dpi(Core::State::AppState& state, Vendor::Windows::UINT new_dpi,
                       const Vendor::Windows::SIZE& window_size) -> void {
  state.app_window->window.dpi = new_dpi;
  state.app_window->d2d_context.needs_font_update = true;

  // 更新布局配置（基于新的DPI）
  UI::AppWindow::Layout::update_layout(state);

  // 更新窗口尺寸
  if (state.app_window->window.hwnd) {
    Vendor::Windows::RECT currentRect{};
    Vendor::Windows::GetWindowRect(state.app_window->window.hwnd, &currentRect);

    Vendor::Windows::SetWindowPos(
        state.app_window->window.hwnd, nullptr, currentRect.left, currentRect.top, window_size.cx,
        window_size.cy, Vendor::Windows::SWP_NOZORDER_t | Vendor::Windows::SWP_NOACTIVATE_t);

    // 如果Direct2D已初始化，调整渲染目标大小
    if (state.app_window->d2d_context.is_initialized) {
      UI::AppWindow::D2DContext::resize_d2d(state, window_size);
    }
  }
}

// 处理 hide 命令
auto handle_hide_event(Core::State::AppState& state) -> void { UI::AppWindow::hide_window(state); }

// 处理 webview 命令
auto handle_webview(Core::State::AppState& state) -> void {
  Logger().info("WebView command received");
  UI::WebViewWindow::toggle_visibility(state);
  Logger().info("WebView window visibility toggled");
}

// 处理退出事件
auto handle_exit_event(Core::State::AppState& state) -> void {
  Logger().info("Exit event received, posting quit message");
  Vendor::Windows::PostQuitMessage(0);
}

// 处理 toggle_visibility 命令
auto handle_toggle_visibility_event(Core::State::AppState& state) -> void {
  UI::AppWindow::toggle_visibility(state);
}

auto register_system_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;
  using namespace UI::AppWindow::Events;

  subscribe<HideEvent>(*app_state.event_bus,
                       [&app_state](const HideEvent&) { handle_hide_event(app_state); });

  subscribe<WebViewEvent>(*app_state.event_bus,
                          [&app_state](const WebViewEvent&) { handle_webview(app_state); });

  subscribe<ExitEvent>(*app_state.event_bus,
                       [&app_state](const ExitEvent&) { handle_exit_event(app_state); });

  subscribe<Core::WebView::Events::WebViewResponseEvent>(
      *app_state.event_bus, [&app_state](const Core::WebView::Events::WebViewResponseEvent& event) {
        try {
          Logger().debug("Processing WebView response on UI thread");

          // 现在我们在UI线程上，可以安全调用WebView API
          Core::WebView::post_message(app_state, event.response);

          Logger().debug("WebView response sent successfully from UI thread");
        } catch (const std::exception& e) {
          Logger().error("Error processing WebView response event: {}", e.what());
        }
      });

  // 注册DPI改变事件处理器
  subscribe<UI::AppWindow::Events::DpiChangeEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::DpiChangeEvent& event) {
        Logger().debug("DPI changed to: {}, window size: {}x{}", event.new_dpi,
                       event.window_size.cx, event.window_size.cy);

        update_render_dpi(app_state, event.new_dpi, event.window_size);

        Logger().info("DPI update completed successfully");
      });

  subscribe<UI::AppWindow::Events::ToggleVisibilityEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::ToggleVisibilityEvent&) {
        handle_toggle_visibility_event(app_state);
      });

  Logger().info("System handlers (including WebView handlers) registered successfully");
}

}  // namespace Handlers