module;

#include <uwebsockets/App.h>

module Core.HttpServer.SseManager;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.Types;
import Utils.Logger;

namespace Core::HttpServer::SseManager {

    auto add_connection(Core::State::AppState& state, void* response_ptr) -> void {
        auto* response = static_cast<uWS::HttpResponse<false>*>(response_ptr);
        
        // 检查状态是否已初始化
        if (!state.http_server) {
            Logger().error("HTTP server state not initialized");
            return;
        }
        
        auto& connections = state.http_server->sse_connections;
        auto& counter = state.http_server->client_counter;
        auto& mtx = state.http_server->sse_connections_mutex;
        
        std::lock_guard<std::mutex> lock(mtx);
        
        auto connection = std::make_shared<Types::SseConnection>();
        connection->response = response;
        connection->client_id = std::to_string(++counter);
        connection->connected_at = std::chrono::system_clock::now();
        
        // 注册连接断开回调
        response->onAborted([response, &state, client_id = connection->client_id]() {
            Logger().info("SSE connection aborted");
            
            // 检查状态是否已初始化
            if (!state.http_server) return;
            
            auto& connections = state.http_server->sse_connections;
            auto& mtx = state.http_server->sse_connections_mutex;
            
            std::lock_guard<std::mutex> lock(mtx);
            auto it = std::remove_if(connections.begin(), connections.end(),
                [&client_id](const std::shared_ptr<Types::SseConnection>& conn) {
                    return conn && conn->client_id == client_id;
                });
            connections.erase(it, connections.end());
            Logger().info("SSE connection removed. Total connections: {}", connections.size());
        });
        
        // 添加到连接列表
        connections.push_back(connection);
        Logger().info("New SSE connection established. Total connections: {}", connections.size());
    }
    
    auto remove_connection(Core::State::AppState& state, const std::string& client_id) -> void {
        // 检查状态是否已初始化
        if (!state.http_server) return;
        
        auto& connections = state.http_server->sse_connections;
        auto& mtx = state.http_server->sse_connections_mutex;
        
        std::lock_guard<std::mutex> lock(mtx);
        
        auto old_size = connections.size();
        auto it = std::remove_if(connections.begin(), connections.end(),
            [&client_id](const std::shared_ptr<Types::SseConnection>& conn) {
                return conn && conn->client_id == client_id;
            });
        connections.erase(it, connections.end());
            
        if (connections.size() < old_size) {
            Logger().info("SSE connection removed. Total connections: {}", connections.size());
        }
    }
    
    auto broadcast_event(Core::State::AppState& state, uWS::App& app, const std::string& event_data) -> void {
        // 检查状态是否已初始化
        if (!state.http_server) return;
        
        auto& connections = state.http_server->sse_connections;
        auto& mtx = state.http_server->sse_connections_mutex;
        
        Logger().debug("Broadcasting event to {} SSE clients", connections.size());
        
        std::lock_guard<std::mutex> lock(mtx);
        
        // 使用erase-remove惯用法清理已关闭的连接并广播事件
        auto it = std::remove_if(connections.begin(), connections.end(),
            [&event_data](const std::shared_ptr<Types::SseConnection>& conn) {
                if (!conn || conn->is_closed) {
                    return true;
                }
                
                try {
                    std::string sse_message = std::format("data: {}\n\n", event_data);
                    conn->response->write(sse_message);
                    return false;
                } catch (const std::exception& e) {
                    Logger().error("Failed to send SSE event to client {}: {}", 
                                  conn->client_id, e.what());
                    conn->is_closed = true;
                    return true;
                }
            });
        connections.erase(it, connections.end());
    }
    
    auto get_connection_count(const Core::State::AppState& state) -> size_t {
        // 检查状态是否已初始化
        if (!state.http_server) return 0;
        
        auto& connections = state.http_server->sse_connections;
        auto& mtx = state.http_server->sse_connections_mutex;
        
        std::lock_guard<std::mutex> lock(mtx);
        return connections.size();
    }
}