#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/state/app_state.hpp"

namespace core::http_server::routes {
// 注册所有路由
auto register_routes(core::AppState& state, uWS::App& app) -> void;
}  // namespace core::http_server::routes
