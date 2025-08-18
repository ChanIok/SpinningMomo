module;

#include <windows.h>

module Utils.Timer;

import std;
import Utils.Logger;

namespace Utils::Timer {

Timer::Timer() { (void)CreateTimerObject(); }

Timer::~Timer() {
  Cancel();
  if (m_timer) {
    CloseHandle(m_timer);
    m_timer = nullptr;
  }
}

auto Timer::CreateTimerObject() -> std::expected<void, TimerError> {
  if (!m_timer) {
    m_timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
    if (!m_timer) {
      Logger().error("Failed to create waitable timer");
      return std::unexpected(TimerError::CreateTimerFailed);
    }
  }
  return {};
}

auto Timer::SetTimer(std::chrono::milliseconds delay, std::function<void()> callback)
    -> std::expected<void, TimerError> {
  // 检查当前状态
  State expectedState = State::Idle;
  if (!m_state.compare_exchange_strong(expectedState, State::Running)) {
    // 如果已经在运行，不允许再次设置
    if (m_state == State::Running) {
      return std::unexpected(TimerError::AlreadyRunning);
    }

    // 如果是已触发状态，重置为空闲状态
    if (m_state == State::Triggered) {
      m_state.store(State::Idle);
    }
  }

  if (!callback) {
    m_state.store(State::Idle);
    return std::unexpected(TimerError::InvalidCallback);
  }

  // 确保定时器对象存在
  if (auto result = CreateTimerObject(); !result) {
    m_state.store(State::Idle);
    return result;
  }

  // 取消任何现有等待
  Cancel();

  // 保存参数
  m_callback = std::move(callback);
  m_delay = delay;

  // 重置并启动定时器
  return Reset();
}

auto Timer::Reset() -> std::expected<void, TimerError> {
  // 只有在有回调和定时器对象的情况下才能重置
  if (!m_timer || !m_callback) {
    return std::unexpected(TimerError::NotInitialized);
  }

  // 确保状态为运行
  State expectedState = State::Idle;
  if (!m_state.compare_exchange_strong(expectedState, State::Running)) {
    // 如果已经触发，需要先重置
    if (m_state == State::Triggered) {
      m_state.store(State::Running);
    }
    // 如果已在运行，无需更改状态
  }

  // 设置定时器
  LARGE_INTEGER dueTime;
  const auto delayMs = static_cast<LONGLONG>(m_delay.count());
  dueTime.QuadPart = -delayMs * 10000LL;  // 转换为100纳秒单位

  if (!SetWaitableTimer(m_timer, &dueTime, 0, nullptr, nullptr, FALSE)) {
    m_state.store(State::Idle);
    return std::unexpected(TimerError::SetTimerFailed);
  }

  // 启动等待线程
  StartWaitThread();
  return {};
}

auto Timer::StartWaitThread() -> void {
  // 捕获必要的变量，避免访问类成员
  HANDLE timerHandle = m_timer;
  auto callback = m_callback;

  m_waitThread = std::jthread([this, timerHandle, callback](std::stop_token token) {
    // 等待定时器信号或停止信号
    DWORD result = WaitForSingleObject(timerHandle, INFINITE);

    // 检查是否被要求停止
    if (token.stop_requested()) {
      return;
    }

    // 只有在等待成功且定时器还在运行状态时才执行回调
    State expectedState = State::Running;
    if (result == WAIT_OBJECT_0 &&
        m_state.compare_exchange_strong(expectedState, State::Triggered)) {
      callback();
    }
  });
  
  // 分离线程使其独立运行，避免在回调中Cancel时发生死锁
  m_waitThread.detach();
}

auto Timer::Cancel() -> void {
  // 设置状态为空闲，不管之前是什么状态
  State previousState = m_state.exchange(State::Idle);

  // 如果定时器正在运行，取消Windows定时器对象
  if (previousState == State::Running && m_timer) {
    CancelWaitableTimer(m_timer);
  }
  
  // 请求停止线程（如果线程还存在的话）
  if (m_waitThread.joinable()) {
    m_waitThread.request_stop();
  }
}

}  // namespace Utils::Timer