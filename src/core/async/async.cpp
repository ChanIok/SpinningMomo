module Core.Async;

import std;
import Core.State;
import Core.Async.State;
import Utils.Logger;
import <asio.hpp>;

namespace Core::Async {

auto start(Core::State::AppState& state, size_t thread_count) -> std::expected<void, std::string> {
  if (!state.async) {
    return std::unexpected("AsyncState is not initialized");
  }
  auto& runtime = *state.async;

  // 检查是否已经运行
  if (runtime.is_running.exchange(true)) {
    Logger().warn("AsyncRuntime already started");
    return std::unexpected("AsyncRuntime already started");
  }

  try {
    // 确定线程数
    if (thread_count == 0) {
      thread_count = std::thread::hardware_concurrency() / 2;
      if (thread_count < 2) thread_count = 2;  // 最少 2 条
    }
    const size_t resolved_thread_count = thread_count;
    runtime.thread_count = resolved_thread_count;

    // 初始化io_context
    runtime.io_context.emplace();

    Logger().info("Starting AsyncRuntime with {} threads", resolved_thread_count);

    // 创建工作线程池
    runtime.worker_threads.reserve(resolved_thread_count);
    for (size_t i = 0; i < resolved_thread_count; ++i) {
      runtime.worker_threads.emplace_back([&runtime, i]() {
        try {
          const auto work = asio::make_work_guard(*runtime.io_context);
          runtime.io_context->run();
        } catch (const std::exception& e) {
          Logger().error("AsyncRuntime worker thread {} error: {}", i, e.what());
        }
      });
    }

    Logger().info("AsyncRuntime started successfully");
    return {};

  } catch (const std::exception& e) {
    // 启动失败，恢复状态
    runtime.is_running = false;
    runtime.io_context.reset();
    runtime.worker_threads.clear();

    auto error_msg = std::format("Failed to start AsyncRuntime: {}", e.what());
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }
}

auto stop(Core::State::AppState& state) -> void {
  if (!state.async) {
    return;
  }
  auto& runtime = *state.async;
  if (!runtime.is_running.exchange(false)) {
    return;  // 已经停止
  }

  Logger().info("Stopping AsyncRuntime");

  try {
    // 标记关闭请求
    runtime.shutdown_requested = true;

    // 停止io_context
    if (runtime.io_context) {
      runtime.io_context->stop();
    }

    // 等待所有工作线程结束
    for (auto& worker : runtime.worker_threads) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    // 清理资源
    runtime.worker_threads.clear();
    runtime.io_context.reset();
    runtime.shutdown_requested = false;

    Logger().info("AsyncRuntime stopped");

  } catch (const std::exception& e) {
    Logger().error("Error during AsyncRuntime shutdown: {}", e.what());
  }
}

auto is_running(const Core::State::AppState& state) -> bool {
  if (!state.async) {
    return false;
  }
  const auto& runtime = *state.async;
  return runtime.is_running.load();
}

auto get_io_context(Core::State::AppState& state) -> asio::io_context* {
  if (!state.async) {
    return nullptr;
  }
  auto& runtime = *state.async;
  if (!runtime.io_context) {
    return nullptr;
  }
  return std::addressof(*runtime.io_context);
}

}  // namespace Core::Async
