module;

#include <windows.h>

module Utils.Timeout;

import std;
import Utils.Logger;

namespace Utils::Timeout {

struct Timeout::shared_state {
  std::mutex mutex;
  std::function<void()> callback;
  // true 表示存在待触发任务；触发/取消后置回 false。
  std::atomic<bool> pending{false};
};

auto CALLBACK Timeout::threadpool_callback(PTP_CALLBACK_INSTANCE, PVOID context, PTP_TIMER)
    -> void {
  auto* shared = static_cast<shared_state*>(context);
  std::function<void()> callback;
  {
    std::lock_guard lock(shared->mutex);
    // 回调触发时先把状态切换为非 pending，再搬运回调，避免竞态下重复执行。
    callback = shared->callback;
    shared->pending.store(false, std::memory_order_release);
    shared->callback = nullptr;
  }

  if (callback) {
    try {
      callback();
    } catch (...) {
      Logger().error("Exception in timeout callback");
    }
  }
}

Timeout::Timeout() {
  m_shared = std::make_unique<shared_state>();
  (void)create_timer_object();
}

Timeout::~Timeout() {
  cancel();
  if (m_timer) {
    CloseThreadpoolTimer(m_timer);
    m_timer = nullptr;
  }
  m_shared.reset();
}

auto Timeout::create_timer_object() -> std::expected<void, timeout_error> {
  if (m_timer) {
    return {};
  }

  auto* timer = CreateThreadpoolTimer(Timeout::threadpool_callback, m_shared.get(), nullptr);
  if (!timer) {
    Logger().error("Failed to create threadpool timer");
    return std::unexpected(timeout_error::create_timer_failed);
  }

  m_timer = timer;
  return {};
}

auto Timeout::set_timeout(std::chrono::milliseconds delay, std::function<void()> callback)
    -> std::expected<void, timeout_error> {
  if (!callback) {
    return std::unexpected(timeout_error::invalid_callback);
  }

  if (auto create_result = create_timer_object(); !create_result) {
    return create_result;
  }

  // 与 JS setTimeout 一致：同一实例再次设置时覆盖上一次任务。
  cancel();

  {
    std::lock_guard lock(m_shared->mutex);
    m_shared->callback = std::move(callback);
    m_shared->pending.store(true, std::memory_order_release);
  }

  ULONGLONG delay_100ns = static_cast<ULONGLONG>(delay.count()) * 10000ULL;
  FILETIME due_time{};
  ULARGE_INTEGER due{};
  due.QuadPart = static_cast<ULONGLONG>(0) - delay_100ns;
  due_time.dwLowDateTime = due.LowPart;
  due_time.dwHighDateTime = due.HighPart;

  // period=0 表示一次性定时器。
  SetThreadpoolTimer(m_timer, &due_time, 0, 0);
  return {};
}

auto Timeout::cancel() -> void {
  if (!m_timer) {
    if (m_shared) {
      std::lock_guard lock(m_shared->mutex);
      m_shared->pending.store(false, std::memory_order_release);
      m_shared->callback = nullptr;
    }
    return;
  }

  // 先取消未来触发，再等待可能正在执行的回调结束，确保析构安全。
  SetThreadpoolTimer(m_timer, nullptr, 0, 0);
  WaitForThreadpoolTimerCallbacks(m_timer, TRUE);

  std::lock_guard lock(m_shared->mutex);
  m_shared->pending.store(false, std::memory_order_release);
  m_shared->callback = nullptr;
}

auto Timeout::is_pending() const -> bool {
  if (!m_shared) {
    return false;
  }
  return m_shared->pending.load(std::memory_order_acquire);
}

}  // namespace Utils::Timeout
