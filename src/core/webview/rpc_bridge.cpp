#include "core/webview/rpc_bridge.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/async/async.hpp"
#include "core/events/events.hpp"
#include "core/rpc/rpc.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/events.hpp"
#include "core/webview/state.hpp"
#include "core/webview/webview.hpp"
#include "utils/logger/logger.hpp"

namespace core::webview::rpc_bridge {

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

auto initialize_rpc_bridge(core::AppState& state) -> void {
  Logger().info("Initializing WebView rpc bridge");

  // 确保异步运行时已启动
  if (!core::async::is_running(state)) {
    Logger().warn("Async runtime not running when initializing RPC bridge");
  }

  // 初始化rpc桥接状态
  state.webview->messaging.next_message_id = 1;

  Logger().info("WebView rpc bridge initialized");
}

auto handle_webview_message(core::AppState& state, const std::string& message)
    -> asio::awaitable<void> {
  Logger().debug("Handling WebView message: {}",
                 message.substr(0, 100) + (message.size() > 100 ? "..." : ""));

  try {
    // 在异步线程上处理rpc请求
    auto response = co_await core::rpc::process_request(state, message);

    // 直接投递响应字符串到UI线程处理
    core::events::post(state, core::webview::events::WebViewResponseEvent{response});

    Logger().debug("WebView response queued for UI thread processing");

  } catch (const std::exception& e) {
    Logger().error("Error handling WebView rpc message: {}", e.what());

    // 错误处理：直接投递错误响应字符串
    core::events::post(state, core::webview::events::WebViewResponseEvent{
                                  create_generic_error_response(e.what())});

    Logger().debug("WebView error response queued for UI thread processing");
  }
}

auto send_notification(core::AppState& state, const std::string& method, const std::string& params)
    -> void {
  // 构造 JSON-RPC 2.0 通知格式
  auto notification = std::format(R"({{
        "jsonrpc": "2.0",
        "method": "{}",
        "params": {}
    }})",
                                  method, params);

  try {
    core::webview::post_message(state, notification);
    Logger().debug("Sent notification: {}", method);
  } catch (const std::exception& e) {
    Logger().error("Failed to send notification '{}': {}", method, e.what());
  }
}

// 创建交给 WebView2 COM 事件适配层的可复制消息回调
auto create_message_handler(core::AppState& state) -> std::function<void(const std::string&)> {
  return [&state](const std::string& message) {
    // COM 回调只负责投递协程，RPC 解析和业务执行都留在异步运行时
    asio::co_spawn(
        *core::async::get_io_context(state),
        [&state, message]() -> asio::awaitable<void> {
          co_await handle_webview_message(state, message);
        },
        core::async::log_completion("WebView RPC request"));
  };
}

}  // namespace core::webview::rpc_bridge
