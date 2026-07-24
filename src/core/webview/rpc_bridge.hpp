#pragma once

#include <asio.hpp>

#include "core/state/app_state.hpp"

namespace Core::WebView::RpcBridge {

// 初始化RPC桥接
auto initialize_rpc_bridge(Core::State::AppState& state) -> void;

// 处理来自前端的RPC消息
auto handle_webview_message(Core::State::AppState& state, const std::string& message)
    -> asio::awaitable<void>;

// 向前端发送通知 (JSON-RPC notification)
auto send_notification(Core::State::AppState& state, const std::string& method,
                       const std::string& params) -> void;

// 创建交给 WebView2/WRL 的可复制消息回调；COM 事件适配层需要复制 callable
auto create_message_handler(Core::State::AppState& state)
    -> std::function<void(const std::string&)>;

}  // namespace Core::WebView::RpcBridge
