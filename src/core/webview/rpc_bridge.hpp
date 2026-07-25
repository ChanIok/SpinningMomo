#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/state/app_state.hpp"

namespace core::webview::rpc_bridge {

// 初始化rpc桥接
auto initialize_rpc_bridge(core::AppState& state) -> void;

// 处理来自前端的rpc消息
auto handle_webview_message(core::AppState& state, const std::string& message)
    -> asio::awaitable<void>;

// 向前端发送通知 (JSON-RPC notification)
auto send_notification(core::AppState& state, const std::string& method, const std::string& params)
    -> void;

// 创建交给 WebView2/WRL 的可复制消息回调；COM 事件适配层需要复制 callable
auto create_message_handler(core::AppState& state) -> std::function<void(const std::string&)>;

}  // namespace core::webview::rpc_bridge
