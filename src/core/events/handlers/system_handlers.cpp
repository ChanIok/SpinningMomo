module;

module Core.Events.Handlers.System;

import std;
import Core.Events;
import Core.State;
import Core.WebView;
import Core.WebView.Events;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import UI.WebViewWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Core::Events::Handlers {

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
      app_state,
      [&app_state](const UI::FloatingWindow::Events::HideEvent&) { handle_hide_event(app_state); });

  subscribe<UI::FloatingWindow::Events::ExitEvent>(
      app_state,
      [&app_state](const UI::FloatingWindow::Events::ExitEvent&) { handle_exit_event(app_state); });

  subscribe<UI::FloatingWindow::Events::ToggleVisibilityEvent>(
      app_state, [&app_state](const UI::FloatingWindow::Events::ToggleVisibilityEvent&) {
        handle_toggle_visibility_event(app_state);
      });

  subscribe<Core::WebView::Events::WebViewResponseEvent>(
      app_state, [&app_state](const Core::WebView::Events::WebViewResponseEvent& event) {
        try {
          // 在UI线程上安全调用WebView API
          Core::WebView::post_message(app_state, event.response);

        } catch (const std::exception& e) {
          Logger().error("Error processing WebView response event: {}", e.what());
        }
      });
}

}  // namespace Core::Events::Handlers
