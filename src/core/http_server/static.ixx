module;

#include <uwebsockets/App.h>

export module Core.HttpServer.Static;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.Types;

namespace Core::HttpServer::Static {

// 读取并发送下一个数据块
auto read_and_send_next_chunk(std::shared_ptr<Types::StreamContext> ctx) -> void;

// 注册自定义路径解析器（接受 AppState）
export auto register_path_resolver(Core::State::AppState& state, std::string prefix,
                                   Types::PathResolver resolver) -> void;

// 注销路径解析器
export auto unregister_path_resolver(Core::State::AppState& state, std::string_view prefix) -> void;

// 注册静态文件路由（作为fallback）
export auto register_routes(Core::State::AppState& state, uWS::App& app) -> void;

}  // namespace Core::HttpServer::Static
