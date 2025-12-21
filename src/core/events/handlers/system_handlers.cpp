module;

module Core.Events.Handlers.System;

import std;
import Core.Events;
import Core.State;
import Core.WebView;
import Core.WebView.Events;
import UI.FloatingWindow;
import UI.FloatingWindow.Layout;
import UI.FloatingWindow.D2DContext;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Events;
import UI.WebViewWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Core::Events::Handlers {

// 从 app_state.ixx 迁移的 DPI 更新函数
auto update_render_dpi(Core::State::AppState& state, Vendor::Windows::UINT new_dpi,
                       const Vendor::Windows::SIZE& window_size) -> void {
  state.floating_window->window.dpi = new_dpi;
  state.floating_window->d2d_context.needs_font_update = true;

  // 更新布局配置（基于新的DPI）
  UI::FloatingWindow::Layout::update_layout(state);

  // 更新窗口尺寸
  if (state.floating_window->window.hwnd) {
    Vendor::Windows::RECT currentRect{};
    Vendor::Windows::GetWindowRect(state.floating_window->window.hwnd, &currentRect);

    Vendor::Windows::SetWindowPos(
        state.floating_window->window.hwnd, nullptr, currentRect.left, currentRect.top,
        window_size.cx, window_size.cy,
        Vendor::Windows::kSWP_NOZORDER | Vendor::Windows::kSWP_NOACTIVATE);

    // 如果Direct2D已初始化，调整渲染目标大小
    if (state.floating_window->d2d_context.is_initialized) {
      UI::FloatingWindow::D2DContext::resize_d2d(state, window_size);
    }
  }
}

// 处理 hide 命令
auto handle_hide_event(Core::State::AppState& state) -> void {
  UI::FloatingWindow::hide_window(state);
}

// 处理退出事件
auto handle_exit_event(Core::State::AppState& state) -> void {
  Logger().info("Exit event received, posting quit message");
  Vendor::Windows::PostQuitMessage(0);
}

// 处理 toggle_visibility 命令
auto handle_toggle_visibility_event(Core::State::AppState& state) -> void {
  UI::FloatingWindow::toggle_visibility(state);
}

auto register_system_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  subscribe<UI::FloatingWindow::Events::HideEvent>(
      *app_state.events,
      [&app_state](const UI::FloatingWindow::Events::HideEvent&) { handle_hide_event(app_state); });

  subscribe<UI::FloatingWindow::Events::ExitEvent>(
      *app_state.events,
      [&app_state](const UI::FloatingWindow::Events::ExitEvent&) { handle_exit_event(app_state); });

  subscribe<UI::FloatingWindow::Events::ToggleVisibilityEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::ToggleVisibilityEvent&) {
        handle_toggle_visibility_event(app_state);
      });

  subscribe<UI::FloatingWindow::Events::DpiChangeEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::DpiChangeEvent& event) {
        Logger().debug("DPI changed to: {}, window size: {}x{}", event.new_dpi,
                       event.window_size.cx, event.window_size.cy);

        update_render_dpi(app_state, event.new_dpi, event.window_size);

        Logger().info("DPI update completed successfully");
      });

  subscribe<Core::WebView::Events::WebViewResponseEvent>(
      *app_state.events, [&app_state](const Core::WebView::Events::WebViewResponseEvent& event) {
        try {
          // 在UI线程上安全调用WebView API
          Core::WebView::post_message(app_state, event.response);

        } catch (const std::exception& e) {
          Logger().error("Error processing WebView response event: {}", e.what());
        }
      });
}

}  // namespace Core::Events::Handlers