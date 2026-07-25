#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/state.hpp"
#include "core/http_server/types.hpp"
#include "core/state/app_state.hpp"

namespace core::http_server::static_content {

// 读取并发送下一个数据块
auto read_and_send_next_chunk(std::shared_ptr<StreamContext> ctx) -> void;

// 注册自定义路径解析器（接受 AppState）
auto register_path_resolver(core::AppState& state, std::string prefix, PathResolver resolver)
    -> void;

// 注销路径解析器
auto unregister_path_resolver(core::AppState& state, std::string_view prefix) -> void;

// 注册静态文件路由（作为fallback）
auto register_routes(core::AppState& state, uWS::App& app) -> void;

}  // namespace core::http_server::static_content
