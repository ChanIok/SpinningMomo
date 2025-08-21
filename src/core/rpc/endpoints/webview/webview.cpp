module;

#include <windows.h>

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

// RPC参数和结果类型定义
struct WindowStateResult {
  bool is_maximized;
};

struct WindowControlResult {
  bool success;
};

// RPC处理函数实现
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

auto handle_maximize_window(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<WindowControlResult>> {
  auto result = UI::WebViewWindow::maximize_window(app_state);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to maximize window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
}

auto handle_restore_window(Core::State::AppState& app_state,
                           [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<WindowControlResult>> {
  auto result = UI::WebViewWindow::restore_window(app_state);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to restore window: " + result.error()});
  }

  co_return WindowControlResult{.success = true};
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
      app_state, app_state.rpc->registry, "webview.maximize", handle_maximize_window,
      "Maximize the webview window");

  Core::RPC::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.restore", handle_restore_window,
      "Restore the webview window");

  Core::RPC::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc->registry, "webview.close", handle_close_window,
      "Close the webview window");
}

}  // namespace Core::RPC::Endpoints::WebView