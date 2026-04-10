module;

export module Utils.Timeout;

import std;
import <windows.h>;

namespace Utils::Timeout {

// 超时错误类型：只保留当前可明确上报的错误。
enum class timeout_error { invalid_callback, create_timer_failed };

export class Timeout {
 public:
  Timeout();
  ~Timeout();

  Timeout(const Timeout&) = delete;
  Timeout& operator=(const Timeout&) = delete;
  Timeout(Timeout&&) = delete;
  Timeout& operator=(Timeout&&) = delete;

  // 设置一次性延迟任务（等价于 setTimeout）。
  // 若已有 pending 任务，会先取消再覆盖。
  auto set_timeout(std::chrono::milliseconds delay, std::function<void()> callback)
      -> std::expected<void, timeout_error>;
  // 取消当前待触发任务；可重复调用（幂等）。
  auto cancel() -> void;
  // 是否存在尚未触发的延迟任务。
  auto is_pending() const -> bool;

 private:
  struct shared_state;
  // Windows 线程池回调入口：在系统线程池线程执行。
  static auto CALLBACK threadpool_callback(PTP_CALLBACK_INSTANCE instance, PVOID context,
                                           PTP_TIMER timer) -> void;
  std::unique_ptr<shared_state> m_shared;
  PTP_TIMER m_timer = nullptr;

  auto create_timer_object() -> std::expected<void, timeout_error>;
};

}  // namespace Utils::Timeout
