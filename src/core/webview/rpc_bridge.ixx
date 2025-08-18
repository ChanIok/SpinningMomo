module;

#include <asio.hpp>

export module Core.WebView.RpcBridge;

import std;
import Core.State;

namespace Core::WebView::RpcBridge {

// 初始化RPC桥接
export auto initialize_rpc_bridge(Core::State::AppState& state) -> void;

// 处理来自前端的RPC消息
export auto handle_webview_message(Core::State::AppState& state, const std::string& message)
    -> asio::awaitable<void>;

// 向前端发送通知 (JSON-RPC notification)
export auto send_notification(Core::State::AppState& state, const std::string& method,
                              const std::string& params) -> void;

// 处理WebView消息的回调函数
export auto create_message_handler(Core::State::AppState& state)
    -> std::function<void(const std::string&)>;

}  // namespace Core::WebView::RpcBridge