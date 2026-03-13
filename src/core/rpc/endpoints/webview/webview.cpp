module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.WebView;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import UI.WebViewWindow;

namespace Core::RPC::Endpoints::WebView {

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

auto handle_minimize_window(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<WindowControlResult>> {
  auto result = UI::WebViewWindow::minimize_window(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to minimize window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_toggle_maximize_window(Core::State::AppState& app_state,
                                   [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<WindowControlResult>> {
  auto result = UI::WebViewWindow::toggle_maximize_window(app_state);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to toggle maximize window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_set_fullscreen_window(Core::State::AppState& app_state,
                                  const SetFullscreenParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<FullscreenControlResult>> {
  auto result = UI::WebViewWindow::set_fullscreen_window(app_state, params.fullscreen);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to set fullscreen window state: " + result.error()});
  }

  co_return FullscreenControlResult{.success = true, .fullscreen = params.fullscreen};
}

auto handle_close_window(Core::State::AppState& app_state,
                         [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<WindowControlResult>> {
  auto result = UI::WebViewWindow::close_window(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to close window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.minimize", handle_minimize_window,
      "Minimize the webview window");

  Core::RPC::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.toggleMaximize", handle_toggle_maximize_window,
      "Toggle maximize state of the webview window");

  Core::RPC::register_method<SetFullscreenParams, FullscreenControlResult>(
      app_state, app_state.rpc->registry, "webview.setFullscreen", handle_set_fullscreen_window,
      "Set fullscreen state of the webview window");

  Core::RPC::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.close", handle_close_window,
      "Close the webview window");
}

}  // namespace Core::RPC::Endpoints::WebView
