#include "core/events/handlers/system_handlers.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/events/events.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/events.hpp"
#include "core/webview/webview.hpp"
#include "ui/floating_window/events.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/webview_window/webview_window.hpp"
#include "utils/logger/logger.hpp"

namespace core::events::handlers {

// 处理 hide 命令
auto handle_hide_event(core::AppState& state) -> void { ui::floating_window::hide_window(state); }

// 处理退出事件
auto handle_exit_event(core::AppState& state) -> void {
  Logger().info("Exit event received, posting quit message");
  PostQuitMessage(0);
}

// 处理 toggle_visibility 命令
auto handle_toggle_visibility_event(core::AppState& state) -> void {
  ui::floating_window::toggle_visibility(state);
}

auto register_system_handlers(core::AppState& app_state) -> void {
  using namespace core::events;

  subscribe<ui::floating_window::events::HideEvent>(
      app_state, [&app_state](const ui::floating_window::events::HideEvent&) {
        handle_hide_event(app_state);
      });

  subscribe<ui::floating_window::events::ExitEvent>(
      app_state, [&app_state](const ui::floating_window::events::ExitEvent&) {
        handle_exit_event(app_state);
      });

  subscribe<ui::floating_window::events::ToggleVisibilityEvent>(
      app_state, [&app_state](const ui::floating_window::events::ToggleVisibilityEvent&) {
        handle_toggle_visibility_event(app_state);
      });

  subscribe<core::webview::events::WebViewResponseEvent>(
      app_state, [&app_state](const core::webview::events::WebViewResponseEvent& event) {
        try {
          // 在UI线程上安全调用webview API
          core::webview::post_message(app_state, event.response);

        } catch (const std::exception& e) {
          Logger().error("Error processing WebView response event: {}", e.what());
        }
      });
}

}  // namespace core::events::handlers
