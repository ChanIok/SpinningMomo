#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::worker_pool {

// 启动工作线程池
auto start(core::AppState& state, size_t thread_count = 0) -> std::expected<void, std::string>;

// 停止工作线程池（优雅关闭）
auto stop(core::AppState& state) -> void;

// 检查线程池是否正在运行
auto is_running(const core::AppState& state) -> bool;

// 提交任务到线程池
auto submit_task(core::AppState& state, std::move_only_function<void()> task) -> bool;

// 获取工作线程数量
auto get_thread_count(const core::AppState& state) -> size_t;

// 获取待处理任务数量
auto get_pending_tasks(core::AppState& state) -> size_t;

}  // namespace core::worker_pool
