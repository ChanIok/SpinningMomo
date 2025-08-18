module;

#include <windows.h>

export module Utils.Timer;

import std;

namespace Utils::Timer {

// 定时器错误类型
enum class TimerError {
  InvalidCallback,
  CreateTimerFailed,
  SetTimerFailed,
  AlreadyRunning,
  NotInitialized
};

// 定时器状态
enum class State {
  Idle,      // 空闲状态，可以设置定时器
  Running,   // 定时器正在运行
  Triggered  // 定时器已触发
};

export class Timer {
 public:
  Timer();
  ~Timer();

  // 禁用拷贝
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  // 设置定时器，只有在 Idle 或 Triggered 状态时才能调用
  auto SetTimer(std::chrono::milliseconds delay, std::function<void()> callback)
      -> std::expected<void, TimerError>;

  // 重置定时器时间，保持相同的回调
  auto Reset() -> std::expected<void, TimerError>;

  // 取消定时器
  auto Cancel() -> void;

  // 获取定时器状态
  auto GetState() const -> State { return m_state; }

  // 检查定时器是否正在运行
  auto IsRunning() const -> bool { return m_state == State::Running; }

 private:
  HANDLE m_timer = nullptr;
  std::atomic<State> m_state{State::Idle};
  std::function<void()> m_callback;
  std::chrono::milliseconds m_delay{0};
  std::jthread m_waitThread;

  // 创建定时器对象
  auto CreateTimerObject() -> std::expected<void, TimerError>;

  // 启动等待线程
  auto StartWaitThread() -> void;
};

}  // namespace Utils::Timer