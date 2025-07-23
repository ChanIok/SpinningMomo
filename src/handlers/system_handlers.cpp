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
import UI.TrayMenu.State;
import Utils.Logger;
import Vendor.Windows;

namespace Handlers {

// 从 app_state.ixx 迁移的 DPI 更新函数
auto update_render_dpi(Core::State::AppState& state, Vendor::Windows::UINT new_dpi,
                       const Vendor::Windows::SIZE& window_size) -> void {
  state.app_window->window.dpi = new_dpi;
  state.app_window->d2d_context.needs_font_update = true;
  state.app_window->layout.update_dpi_scaling(new_dpi);
  state.tray_menu->layout.update_dpi_scaling(new_dpi);

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

auto register_system_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册系统命令事件处理器
  subscribe<UI::AppWindow::Events::SystemCommandEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::SystemCommandEvent& event) {
        Logger().debug("System command received: {}", event.command);

        if (event.command == "toggle_visibility") {
          UI::AppWindow::toggle_visibility(app_state);
        } else if (event.command == "webview_test") {
          // 处理webview测试命令 - 切换WebView窗口显示状态
          Logger().info("WebView test command received");

          if (UI::WebViewWindow::is_visible(app_state)) {
            // 隐藏WebView窗口
            UI::WebViewWindow::hide(app_state);
            Core::WebView::hide_webview(app_state);
            Logger().info("WebView window hidden");
          } else {
            // 显示WebView窗口
            if (auto window_result = UI::WebViewWindow::show(app_state); window_result) {
              // 等待一下让窗口完全显示
              std::this_thread::sleep_for(std::chrono::milliseconds(500));

              // 检查WebView状态并重试
              int retry_count = 0;
              const int max_retries = 10;

              while (retry_count < max_retries && !Core::WebView::is_webview_ready(app_state)) {
                Logger().debug("Waiting for WebView to be ready... (attempt {}/{})",
                               retry_count + 1, max_retries);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                retry_count++;
              }

              if (Core::WebView::is_webview_ready(app_state)) {
                if (auto webview_result = Core::WebView::show_webview(app_state); webview_result) {
                  Logger().info("WebView window and content shown successfully");
                } else {
                  Logger().error("Failed to show WebView content: {}", webview_result.error());
                }
              } else {
                Logger().error(
                    "WebView still not ready after {} attempts. Check WebView initialization.",
                    max_retries);
              }
            } else {
              Logger().error("Failed to show WebView window: {}", window_result.error());
            }
          }
        }
      });

  // 注册WebView响应事件处理器
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

  Logger().info("System handlers (including WebView handlers) registered successfully");
}

}  // namespace Handlers