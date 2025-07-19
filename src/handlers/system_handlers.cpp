module;

module Handlers.System;

import std;
import Core.Events;
import Core.State;
import Core.WebView;
import UI.AppWindow;
import UI.WebViewWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Handlers {

auto register_system_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册系统命令事件处理器
  subscribe(*app_state.event_bus, EventType::SystemCommand, [&app_state](const Event& event) {
    auto command = std::any_cast<std::string>(event.data);
    Logger().debug("System command received: {}", command);

    if (command == "toggle_visibility") {
      UI::AppWindow::toggle_visibility(app_state);
    } else if (command == "webview_test") {
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
            Logger().debug("Waiting for WebView to be ready... (attempt {}/{})", retry_count + 1, max_retries);
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
            Logger().error("WebView still not ready after {} attempts. Check WebView initialization.", max_retries);
          }
        } else {
          Logger().error("Failed to show WebView window: {}", window_result.error());
        }
      }
    }
  });
}

} 