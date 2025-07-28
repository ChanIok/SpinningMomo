module;

#include <windows.h>

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module UI.WebViewWindow.Rpc;

import std;
import Core.State;
import Core.RpcHandlers;
import Core.RpcHandlers.State;
import Core.RpcHandlers.Types;
import UI.WebViewWindow;
import Utils.Logger;

namespace UI::WebViewWindow::Rpc {

// RPC处理函数实现
auto handle_drag_start(Core::State::AppState& app_state, const DragParams& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::start_drag_window(app_state, params.x, params.y);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to start drag: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in drag start: " + std::string(e.what())});
  }
}

auto handle_drag_move(Core::State::AppState& app_state, const DragParams& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::move_drag_window(app_state, params.x, params.y);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to move window: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in drag move: " + std::string(e.what())});
  }
}

auto handle_drag_end([[maybe_unused]] Core::State::AppState& app_state,
                     [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::end_drag_window(app_state);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to end drag: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in drag end: " + std::string(e.what())});
  }
}

auto handle_minimize_window(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::minimize_window(app_state);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to minimize window: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in minimize window: " + std::string(e.what())});
  }
}

auto handle_maximize_window(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::maximize_window(app_state);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to maximize window: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in maximize window: " + std::string(e.what())});
  }
}

auto handle_restore_window(Core::State::AppState& app_state,
                           [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::restore_window(app_state);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to restore window: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in restore window: " + std::string(e.what())});
  }
}

auto handle_close_window(Core::State::AppState& app_state,
                         [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<WindowControlResult>> {
  try {
    auto result = UI::WebViewWindow::close_window(app_state);

    if (result) {
      co_return WindowControlResult{.success = true};
    } else {
      co_return std::unexpected(Core::RpcHandlers::RpcError{
          .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
          .message = "Failed to close window: " + result.error()});
    }
  } catch (const std::exception& e) {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Exception in close window: " + std::string(e.what())});
  }
}

auto register_handlers(Core::State::AppState& app_state) -> void {
  Core::RpcHandlers::register_method<DragParams, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.dragStart", handle_drag_start,
      "Start dragging the webview window");

  Core::RpcHandlers::register_method<DragParams, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.dragMove", handle_drag_move,
      "Move the webview window during dragging");

  Core::RpcHandlers::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.dragEnd", handle_drag_end,
      "End dragging the webview window");

  Core::RpcHandlers::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.minimize", handle_minimize_window,
      "Minimize the webview window");

  Core::RpcHandlers::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.maximize", handle_maximize_window,
      "Maximize the webview window");

  Core::RpcHandlers::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.restore", handle_restore_window,
      "Restore the webview window");

  Core::RpcHandlers::register_method<rfl::Generic, WindowControlResult>(
      app_state, app_state.rpc_handlers->registry, "webview.close", handle_close_window,
      "Close the webview window");

  Logger().info("Registered webview window RPC methods");
}

}  // namespace UI::WebViewWindow::Rpc