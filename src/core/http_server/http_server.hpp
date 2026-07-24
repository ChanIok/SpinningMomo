#pragma once

#include "core/state/app_state.hpp"

namespace Core::HttpServer {
// 初始化HTTP服务器
auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

// 关闭服务器
auto shutdown(Core::State::AppState& state) -> void;

// 获取SSE连接数量
auto get_sse_connection_count(const Core::State::AppState& state) -> size_t;
}  // namespace Core::HttpServer