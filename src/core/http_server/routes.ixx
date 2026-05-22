module;

export module Core.HttpServer.Routes;

import std;
import Core.State;
import Vendor.UWebSockets;

namespace Core::HttpServer::Routes {
// 注册所有路由
export auto register_routes(Core::State::AppState& state, Vendor::UWebSockets::App& app) -> void;
}  // namespace Core::HttpServer::Routes
