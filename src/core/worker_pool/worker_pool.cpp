module;

module Core.WorkerPool;

import std;
import Core.WorkerPool.State;
import Utils.Logger;

namespace Core::WorkerPool {

auto start(Core::WorkerPool::State::WorkerPoolState& pool, size_t thread_count)
    -> std::expected<void, std::string> {
  // 检查是否已经运行
  if (pool.is_running.exchange(true)) {
    Logger().warn("WorkerPool already started");
    return std::unexpected("WorkerPool already started");
  }

  try {
    // 确定线程数
    if (thread_count == 0) {
      thread_count = std::thread::hardware_concurrency();
      if (thread_count == 0) thread_count = 2;  // 备用值
    }

    Logger().info("Starting WorkerPool with {} threads", thread_count);

    // 创建工作线程池
    pool.worker_threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
      pool.worker_threads.emplace_back([&pool, i]() {
        try {
          // 工作线程主循环
          while (!pool.shutdown_requested.load()) {
            std::function<void()> task;

            // 从任务队列获取任务
            {
              std::unique_lock<std::mutex> lock(pool.queue_mutex);
              pool.condition.wait(lock, [&pool] {
                return pool.shutdown_requested.load() || !pool.task_queue.empty();
              });

              // 如果收到关闭信号且队列为空，退出线程
              if (pool.shutdown_requested.load() && pool.task_queue.empty()) {
                break;
              }

              // 获取任务
              if (!pool.task_queue.empty()) {
                task = std::move(pool.task_queue.front());
                pool.task_queue.pop();
              }
            }

            // 执行任务
            if (task) {
              try {
                task();
              } catch (const std::exception& e) {
                Logger().error("WorkerPool task execution error: {}", e.what());
              } catch (...) {
                Logger().error("WorkerPool task execution unknown error");
              }
            }
          }
        } catch (const std::exception& e) {
          Logger().error("WorkerPool worker thread {} error: {}", i, e.what());
        }
      });
    }

    Logger().info("WorkerPool started successfully");
    return {};

  } catch (const std::exception& e) {
    // 启动失败，恢复状态
    pool.is_running = false;
    pool.worker_threads.clear();

    auto error_msg = std::format("Failed to start WorkerPool: {}", e.what());
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }
}

auto stop(Core::WorkerPool::State::WorkerPoolState& pool) -> void {
  if (!pool.is_running.exchange(false)) {
    return;  // 已经停止
  }

  Logger().info("Stopping WorkerPool");

  try {
    // 标记关闭请求
    pool.shutdown_requested = true;

    // 唤醒所有工作线程
    pool.condition.notify_all();

    // 等待所有工作线程结束
    for (auto& worker : pool.worker_threads) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    // 清理资源
    pool.worker_threads.clear();
    {
      std::lock_guard<std::mutex> lock(pool.queue_mutex);
      // 清空任务队列
      std::queue<std::function<void()>> empty;
      pool.task_queue.swap(empty);
    }
    pool.shutdown_requested = false;

    Logger().info("WorkerPool stopped");

  } catch (const std::exception& e) {
    Logger().error("Error during WorkerPool shutdown: {}", e.what());
  }
}

auto is_running(const Core::WorkerPool::State::WorkerPoolState& pool) -> bool {
  return pool.is_running.load();
}

auto submit_task(Core::WorkerPool::State::WorkerPoolState& pool, std::function<void()> task)
    -> bool {
  if (!pool.is_running.load()) {
    return false;  // 线程池未运行
  }

  if (pool.shutdown_requested.load()) {
    return false;  // 正在关闭，不接受新任务
  }

  try {
    {
      std::lock_guard<std::mutex> lock(pool.queue_mutex);
      pool.task_queue.push(std::move(task));
    }
    pool.condition.notify_one();
    return true;
  } catch (const std::exception& e) {
    Logger().error("Failed to submit task to WorkerPool: {}", e.what());
    return false;
  }
}

auto get_thread_count(const Core::WorkerPool::State::WorkerPoolState& pool) -> size_t {
  return pool.worker_threads.size();
}

auto get_pending_tasks(const Core::WorkerPool::State::WorkerPoolState& pool) -> size_t {
  std::lock_guard<std::mutex> lock(pool.queue_mutex);
  return pool.task_queue.size();
}

}  // namespace Core::WorkerPool
