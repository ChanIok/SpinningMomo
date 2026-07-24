#pragma once

#include <uwebsockets/App.h>

#include "core/state/app_state.hpp"

namespace Core::HttpServer::Routes {
// 注册所有路由
auto register_routes(Core::State::AppState& state, uWS::App& app) -> void;
}  // namespace Core::HttpServer::Routes
