#include "core/rpc/endpoints/webview/webview.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/rfl.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/state.hpp"
#include "ui/webview_window/webview_window.hpp"

namespace core::rpc::endpoints::webview {

struct WindowControlResult {
  bool success;
};

struct SetFullscreenParams {
  bool fullscreen;
};

struct FullscreenControlResult {
  bool success;
  bool fullscreen;
};

struct WindowStateResult {
  bool maximized;
  bool fullscreen;
};

auto handle_minimize_window(core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<WindowControlResult>> {
  auto result = ui::webview_window::minimize_window(app_state);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to minimize window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_toggle_maximize_window(core::AppState& app_state,
                                   [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<WindowControlResult>> {
  auto result = ui::webview_window::toggle_maximize_window(app_state);
  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to toggle maximize window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_set_fullscreen_window(core::AppState& app_state, const SetFullscreenParams& params)
    -> asio::awaitable<core::rpc::RpcResult<FullscreenControlResult>> {
  auto result = ui::webview_window::set_fullscreen_window(app_state, params.fullscreen);
  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to set fullscreen window state: " + result.error()});
  }

  co_return FullscreenControlResult{.success = true, .fullscreen = params.fullscreen};
}

auto handle_close_window(core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<WindowControlResult>> {
  auto result = ui::webview_window::close_window(app_state);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to close window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_get_window_state(core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<WindowStateResult>> {
  co_return WindowStateResult{
      .maximized = app_state.webview && app_state.webview->window.is_maximized,
      .fullscreen = app_state.webview && app_state.webview->window.is_fullscreen,
  };
}

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<rfl::Generic, WindowStateResult>(
      app_state, app_state.rpc->registry, "webview.getWindowState", handle_get_window_state,
      "Get current window state for the webview host window");

  core::rpc::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.minimize", handle_minimize_window,
      "Minimize the webview window");

  core::rpc::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.toggleMaximize", handle_toggle_maximize_window,
      "Toggle maximize state of the webview window");

  core::rpc::register_method<SetFullscreenParams, FullscreenControlResult>(
      app_state, app_state.rpc->registry, "webview.setFullscreen", handle_set_fullscreen_window,
      "Set fullscreen state of the webview window");

  core::rpc::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.close", handle_close_window,
      "Close the webview window");
}

}  // namespace core::rpc::endpoints::webview
