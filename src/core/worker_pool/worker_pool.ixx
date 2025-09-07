module;

export module Core.WorkerPool;

import std;
import Core.WorkerPool.State;

namespace Core::WorkerPool {

// 启动工作线程池
export auto start(Core::WorkerPool::State::WorkerPoolState& pool, size_t thread_count = 0)
    -> std::expected<void, std::string>;

// 停止工作线程池（优雅关闭）
export auto stop(Core::WorkerPool::State::WorkerPoolState& pool) -> void;

// 检查线程池是否正在运行
export auto is_running(const Core::WorkerPool::State::WorkerPoolState& pool) -> bool;

// 提交任务到线程池
export auto submit_task(Core::WorkerPool::State::WorkerPoolState& pool, std::function<void()> task)
    -> bool;

// 获取工作线程数量
export auto get_thread_count(const Core::WorkerPool::State::WorkerPoolState& pool) -> size_t;

// 获取待处理任务数量
export auto get_pending_tasks(const Core::WorkerPool::State::WorkerPoolState& pool) -> size_t;

}  // namespace Core::WorkerPool
