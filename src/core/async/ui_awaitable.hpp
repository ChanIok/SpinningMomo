#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/async/async.hpp"

namespace core::async {

// 用于存储定时器 ID 到协程句柄的映射
inline std::unordered_map<UINT_PTR, std::coroutine_handle<>>& get_timer_handles() {
  static std::unordered_map<UINT_PTR, std::coroutine_handle<>> handles;
  return handles;
}

// 定时器回调函数
inline VOID CALLBACK ui_timer_proc(HWND, UINT, UINT_PTR id, DWORD) {
  auto& handles = get_timer_handles();
  auto it = handles.find(id);
  if (it != handles.end()) {
    auto handle = it->second;
    handles.erase(it);
    KillTimer(nullptr, id);
    handle.resume();  // 恢复协程，在 UI 线程执行
  }
}

// UI 线程延迟等待的 awaitable
// 使用 Windows 原生定时器，完全事件驱动，无轮询
struct ui_delay {
  std::chrono::milliseconds duration;

  // 如果延迟 <= 0，立即完成
  inline bool await_ready() const noexcept { return duration.count() <= 0; }

  // 挂起协程，设置 Windows 定时器
  inline void await_suspend(std::coroutine_handle<> h) const {
    UINT_PTR timer_id = SetTimer(nullptr, 0, static_cast<UINT>(duration.count()), ui_timer_proc);

    if (timer_id == 0) {
      // SetTimer 失败，立即恢复协程
      h.resume();
      return;
    }

    // 记录映射关系
    get_timer_handles()[timer_id] = h;
  }

  // 恢复时无返回值
  inline void await_resume() const noexcept {}
};

// UI 线程协程的返回类型
// 协程立即开始执行，完成后自动清理
struct ui_task {
  struct promise_type {
    ui_task get_return_object() noexcept { return {}; }
    inline std::suspend_never initial_suspend() noexcept { return {}; }  // 立即开始
    inline std::suspend_never final_suspend() noexcept { return {}; }    // 完成后不挂起
    inline void return_void() noexcept {}
    inline void unhandled_exception() noexcept {
      core::async::log_exception("UI task", std::current_exception());
    }
  };
};

}  // namespace core::async
