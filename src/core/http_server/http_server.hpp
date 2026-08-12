#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::http_server {
// 初始化HTTP服务器
auto initialize(core::AppState& state) -> std::expected<void, std::string>;

// 在 HTTP 事件循环线程内切换监听范围，不重启应用或重新注册路由。
auto rebind_listen_socket(core::AppState& state, bool lan_enabled)
    -> std::expected<void, std::string>;

// 关闭服务器
auto shutdown(core::AppState& state) -> void;

// 获取SSE连接数量
auto get_sse_connection_count(const core::AppState& state) -> size_t;
}  // namespace core::http_server
