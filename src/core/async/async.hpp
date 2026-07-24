#pragma once

#include <asio.hpp>

#include "core/state/app_state.hpp"

namespace Core::Async {

// 启动异步运行时（包含初始化）
auto start(Core::State::AppState& state, size_t thread_count = 0)
    -> std::expected<void, std::string>;

// 停止异步运行时（包含清理）
auto stop(Core::State::AppState& state) -> void;

// 检查运行时是否正在运行
auto is_running(const Core::State::AppState& state) -> bool;

// 获取io_context用于提交任务
auto get_io_context(Core::State::AppState& state) -> asio::io_context*;

}  // namespace Core::Async
