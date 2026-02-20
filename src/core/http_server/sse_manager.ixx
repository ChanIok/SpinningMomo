module;

#include <uwebsockets/App.h>

export module Core.HttpServer.SseManager;

import std;
import Core.State;

namespace Core::HttpServer::SseManager {
// 添加 SSE 连接
export auto add_connection(Core::State::AppState& state, uWS::HttpResponse<false>* response)
    -> void;

// 移除 SSE 连接
export auto remove_connection(Core::State::AppState& state, const std::string& client_id) -> void;

// 广播事件到所有 SSE 客户端（线程安全，内部会切换到 HTTP loop 线程）
export auto broadcast_event(Core::State::AppState& state, const std::string& event_data) -> void;

// 获取 SSE 连接数量
export auto get_connection_count(const Core::State::AppState& state) -> size_t;
}  // namespace Core::HttpServer::SseManager
