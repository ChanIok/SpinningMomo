#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

namespace core::async {

struct AsyncState {
  // 核心asio状态
  std::optional<asio::io_context> io_context;
  std::vector<std::jthread> worker_threads;

  // 运行状态
  std::atomic<bool> is_running{false};
  std::atomic<bool> shutdown_requested{false};

  // 配置
  size_t thread_count = 0;  // 0表示使用硬件并发数
};

}  // namespace core::async
