#pragma once

#include <uwebsockets/App.h>

#include "core/state/app_state.hpp"

namespace Core::HttpServer::SseManager {
// 添加 SSE 连接
auto add_connection(Core::State::AppState& state, uWS::HttpResponse<false>* response,
                    std::string allowed_origin = "") -> void;

// 移除 SSE 连接
auto remove_connection(Core::State::AppState& state, const std::string& client_id) -> void;

// 关闭所有 SSE 连接（应在 HTTP loop 线程调用）
auto close_all_connections(Core::State::AppState& state) -> void;

// 广播事件到所有 SSE 客户端（线程安全，内部会切换到 HTTP loop 线程）
auto broadcast_event(Core::State::AppState& state, const std::string& event_data) -> void;

// 获取 SSE 连接数量
auto get_connection_count(const Core::State::AppState& state) -> size_t;
}  // namespace Core::HttpServer::SseManager
