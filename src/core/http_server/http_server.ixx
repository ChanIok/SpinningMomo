module;

export module Core.HttpServer;

import std;
import Core.State;

namespace Core::HttpServer {
    // 初始化HTTP服务器
    export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
    
    // 关闭服务器
    export auto shutdown(Core::State::AppState& state) -> void;
    
    // 获取SSE连接数量
    export auto get_sse_connection_count(const Core::State::AppState& state) -> size_t;
}