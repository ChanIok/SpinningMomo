module;

export module Core.HttpServer.Routes;

import std;
import Core.State;
import <uwebsockets/App.h>;

namespace Core::HttpServer::Routes {
// 注册所有路由
export auto register_routes(Core::State::AppState& state, uWS::App& app) -> void;
}  // namespace Core::HttpServer::Routes
