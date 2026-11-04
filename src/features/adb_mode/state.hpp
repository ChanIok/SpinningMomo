#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/device_session.hpp"
#include "features/adb_mode/recording.hpp"
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

  // 会话状态随模块存在；连接、服务与传输资源由 ADB 队列原地打开和关闭。
  session::DeviceSession device_session;
  // 空闲时为空；每段原地构造，工作线程退出并完成收尾后销毁。
  std::optional<recording::AdbRecordingSession> recording_session;
  std::uint64_t next_recording_id = 0;
  // 只由录制开关请求管理，覆盖请求排队与执行，防止连按重复提交。
  bool recording_control_pending = false;
  std::string last_error;

  // 仅表示本次进程会话中是否成功改写过显示状态，不持久化到磁盘。
  bool restore_pending = false;
};

}  // namespace features::adb_mode
