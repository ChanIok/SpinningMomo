module;

#include <uwebsockets/App.h>

export module Core.HttpServer.SseManager;

import std;
import Core.State;

namespace Core::HttpServer::SseManager {
    // 添加SSE连接
    export auto add_connection(Core::State::AppState& state, void* response) -> void;
    
    // 移除SSE连接
    export auto remove_connection(Core::State::AppState& state, const std::string& client_id) -> void;
    
    // 广播事件到所有SSE客户端
    export auto broadcast_event(Core::State::AppState& state, uWS::App& app, const std::string& event_data) -> void;
    
    // 获取SSE连接数量
    export auto get_connection_count(const Core::State::AppState& state) -> size_t;
}