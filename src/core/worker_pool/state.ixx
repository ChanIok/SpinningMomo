module;

export module Core.WorkerPool.State;

import std;

export namespace Core::WorkerPool::State {

struct WorkerPoolState {
  // 工作线程池
  std::vector<std::jthread> worker_threads;

  // 任务队列和同步原语
  std::queue<std::function<void()>> task_queue;
  std::mutex queue_mutex;
  std::condition_variable condition;

  // 运行状态
  std::atomic<bool> is_running{false};
  std::atomic<bool> shutdown_requested{false};
};

}  // namespace Core::WorkerPool::State
