#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/device_session.hpp"
#include "features/adb_mode/types.hpp"

namespace features::adb_mode {

struct AdbModeState {
  // 状态快照锁；耗时 ADB 操作不持有这把锁，避免状态查询被阻塞。
  mutable std::mutex mutex;

  // 所有 ADB 操作进入同一个 FIFO 队列，由专用线程依次执行。
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::queue<std::move_only_function<void()>> task_queue;
  std::jthread worker_thread;
  std::atomic<std::size_t> pending_tasks{0};
  bool accepting_tasks = false;

  // 对外报告的连接生命周期状态。
  ConnectionState connection_state = ConnectionState::Disconnected;
  // 表示队列中仍有未完成的 ADB 操作。
  bool operation_in_progress = false;

  // 一个完整设备会话统一拥有 ADB 连接、Android 捕获服务、forward 和协议 socket。
  std::shared_ptr<session::DeviceSession> device_session;
  std::string last_error;

  // 仅表示本次进程会话中是否成功改写过显示状态，不持久化到磁盘。
  bool restore_pending = false;
};

}  // namespace features::adb_mode
