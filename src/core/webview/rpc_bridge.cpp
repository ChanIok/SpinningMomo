module;

#include <asio.hpp>

module Core.WebView.RpcBridge;

import std;
import Core.State;
import Core.WebView;
import Core.WebView.State;
import Core.RpcHandlers;
import Core.Async.Runtime;
import Utils.Logger;

namespace Core::WebView::RpcBridge {

auto initialize_rpc_bridge(Core::State::AppState& state) -> void {
  Logger().info("Initializing WebView RPC bridge");

  // 确保异步运行时已启动
  if (!Core::Async::is_running(state)) {
    Logger().warn("Async runtime not running when initializing RPC bridge");
  }

  // 初始化RPC桥接状态
  state.webview.messaging.next_message_id = 1;

  Logger().info("WebView RPC bridge initialized");
}

auto handle_webview_message(Core::State::AppState& state, const std::string& message)
    -> asio::awaitable<void> {
  Logger().debug("Handling WebView message: {}",
                 message.substr(0, 100) + (message.size() > 100 ? "..." : ""));

  try {
    // 处理JSON-RPC请求
    auto response = co_await Core::RpcHandlers::process_request(state, message);

    // 发送响应给前端
    Core::WebView::post_message(state, response);

    Logger().debug("Successfully processed RPC request and sent response");

  } catch (const std::exception& e) {
    Logger().error("Error handling WebView RPC message: {}", e.what());

    // 发送错误响应
    try {
      auto error_response = R"({
                "jsonrpc": "2.0",
                "error": {
                    "code": -32603,
                    "message": "Internal error: )" +
                            std::string(e.what()) + R"("
                },
                "id": null
            })";

      Core::WebView::post_message(state, error_response);
    } catch (...) {
      Logger().error("Failed to send error response");
    }
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
    auto* io_context = Core::Async::get_io_context(state);
    if (!io_context) {
      Logger().error("Async runtime not available for message handling");
      return;
    }

    asio::co_spawn(
        *io_context,
        [&state, message]() -> asio::awaitable<void> {
          co_await handle_webview_message(state, message);
        },
        [](std::exception_ptr e) {
          if (e) {
            try {
              std::rethrow_exception(e);
            } catch (const std::exception& ex) {
              Logger().error("Unhandled exception in message handler: {}", ex.what());
            }
          }
        });
  };
}

}  // namespace Core::WebView::RpcBridge