#pragma once

#include "vendor/std.hpp"

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

  // 连接成功后固定下来的配置，后续显示操作都复用它。
  std::optional<AdbConnectionConfig> active_config;
  // 当前连接是否由本模块执行 adb connect 建立，只有这种连接由模块负责断开。
  bool connection_owned = false;
  std::string last_error;

  // 连接时读取的设备物理尺寸，恢复时始终回到这个尺寸。
  std::optional<Resolution> physical_display;
  // 最近一次成功设置的尺寸；只用于状态展示和后续诊断。
  std::optional<Resolution> current_display;
  // 仅表示本次进程会话中是否成功改写过显示状态，不持久化到磁盘。
  bool restore_pending = false;
};

}  // namespace features::adb_mode
