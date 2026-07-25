#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/state/app_state.hpp"

namespace core::http_server::sse_manager {
// 添加 SSE 连接
auto add_connection(core::AppState& state, uWS::HttpResponse<false>* response,
                    std::string allowed_origin = "") -> void;

// 移除 SSE 连接
auto remove_connection(core::AppState& state, const std::string& client_id) -> void;

// 关闭所有 SSE 连接（应在 HTTP loop 线程调用）
auto close_all_connections(core::AppState& state) -> void;

// 广播事件到所有 SSE 客户端（线程安全，内部会切换到 HTTP loop 线程）
auto broadcast_event(core::AppState& state, const std::string& event_data) -> void;

// 获取 SSE 连接数量
auto get_connection_count(const core::AppState& state) -> size_t;
}  // namespace core::http_server::sse_manager
