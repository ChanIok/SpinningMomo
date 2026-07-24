#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::overlay::threads {

// 启动所有工作线程
auto start_threads(core::AppState& state) -> std::expected<void, std::string>;

// 停止所有线程
auto stop_threads(core::AppState& state) -> void;

// 等待所有线程结束
auto wait_for_threads(core::AppState& state) -> void;

// 钩子线程处理函数
auto hook_thread_proc(core::AppState& state, std::stop_token token) -> void;

// 窗口管理线程处理函数
auto window_manager_thread_proc(core::AppState& state, std::stop_token token) -> void;

}  // namespace features::overlay::threads
