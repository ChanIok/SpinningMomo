module;

#include <asio.hpp>

export module Core.Async.State;

import std;

export namespace Core::Async::State {

struct AsyncRuntimeState {
  // 核心asio状态
  std::unique_ptr<asio::io_context> io_context;
  std::vector<std::jthread> worker_threads;

  // 运行状态
  std::atomic<bool> is_running{false};
  std::atomic<bool> shutdown_requested{false};

  // 配置
  size_t thread_count = 0;  // 0表示使用硬件并发数

  };

}  // namespace Core::Async::State