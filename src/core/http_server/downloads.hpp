#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/state/app_state.hpp"

namespace core::http_server::downloads {

// 注册原始资产和一次性归档的 HTTP 下载路由。
auto register_routes(core::AppState& state, uWS::App& app) -> void;

}  // namespace core::http_server::downloads
