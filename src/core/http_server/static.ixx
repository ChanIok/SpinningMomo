module;

#include <uwebsockets/App.h>

export module Core.HttpServer.Static;

import std;
import Core.State;

namespace Core::HttpServer::Static {
// 注册静态文件路由（作为fallback）
export auto register_routes(Core::State::AppState& state, uWS::App& app) -> void;
}  // namespace Core::HttpServer::Static