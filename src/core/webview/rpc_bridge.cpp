module;

#include <asio.hpp>

module Core.WebView.RpcBridge;

import std;
import Core.State;
import Core.Events;
import Core.WebView;
import Core.WebView.State;
import Core.RpcHandlers;
import Core.Async.Runtime;
import Core.WebView.Events;
import Utils.Logger;

namespace Core::WebView::RpcBridge {

// 辅助函数：创建通用错误响应（当无法处理请求时）
auto create_generic_error_response(const std::string& error_message) -> std::string {
  return std::format(R"({{
    "jsonrpc": "2.0",
    "error": {{
      "code": -32603,
      "message": "Internal error: {}"
    }},
    "id": null
  }})",
                     error_message);
}

auto initialize_rpc_bridge(Core::State::AppState& state) -> void {
  Logger().info("Initializing WebView RPC bridge");

  // 确保异步运行时已启动
  if (!Core::Async::is_running(*state.async_runtime)) {
    Logger().warn("Async runtime not running when initializing RPC bridge");
  }

  // 初始化RPC桥接状态
  state.webview->messaging.next_message_id = 1;

  Logger().info("WebView RPC bridge initialized");
}

auto handle_webview_message(Core::State::AppState& state, const std::string& message)
    -> asio::awaitable<void> {
  Logger().debug("Handling WebView message: {}",
                 message.substr(0, 100) + (message.size() > 100 ? "..." : ""));

  try {
    // 在异步线程上处理RPC请求
    auto response = co_await Core::RpcHandlers::process_request(state, message);

    // 直接投递响应字符串到UI线程处理
    Core::Events::post(*state.event_bus, Core::WebView::Events::WebViewResponseEvent{response});

    Logger().debug("WebView response queued for UI thread processing");

  } catch (const std::exception& e) {
    Logger().error("Error handling WebView RPC message: {}", e.what());

    // 错误处理：直接投递错误响应字符串
    Core::Events::post(*state.event_bus, Core::WebView::Events::WebViewResponseEvent{
                                             create_generic_error_response(e.what())});

    Logger().debug("WebView error response queued for UI thread processing");
  }
}

auto send_notification(Core::State::AppState& state, const std::string& method,
                       const std::string& params) -> void {
  // 构造JSON-RPC 2.0通知格式
  auto notification = std::format(R"({{
        "jsonrpc": "2.0",
        "method": "{}",
        "params": {}
    }})",
                                  method, params);

  try {
    Core::WebView::post_message(state, notification);
    Logger().debug("Sent notification: {}", method);
  } catch (const std::exception& e) {
    Logger().error("Failed to send notification '{}': {}", method, e.what());
  }
}

auto create_message_handler(Core::State::AppState& state)
    -> std::function<void(const std::string&)> {
  return [&state](const std::string& message) {
    // 在异步运行时中处理消息
    asio::co_spawn(
        *Core::Async::get_io_context(*state.async_runtime),
        [&state, message]() -> asio::awaitable<void> {
          co_await handle_webview_message(state, message);
        },
        asio::detached);
  };
}

}  // namespace Core::WebView::RpcBridge