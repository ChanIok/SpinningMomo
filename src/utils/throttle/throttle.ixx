module;

export module Utils.Throttle;

import std;

export namespace Utils::Throttle {

// 节流状态（带参数版本）
template <typename... Args>
struct ThrottleState {
  std::chrono::milliseconds interval{16};                // 节流间隔，默认约60fps
  std::chrono::steady_clock::time_point last_call_time;  // 上次执行时间
  bool has_pending{false};                               // 是否有待处理的调用
  std::tuple<Args...> pending_args;                      // 待处理的参数
  std::mutex mutex;                                      // 线程安全
};

// 节流状态（无参数特化版本）
template <>
struct ThrottleState<void> {
  std::chrono::milliseconds interval{16};
  std::chrono::steady_clock::time_point last_call_time;
  bool has_pending{false};
  std::mutex mutex;
};

// 创建节流状态
template <typename... Args>
auto create(std::chrono::milliseconds interval) -> std::unique_ptr<ThrottleState<Args...>> {
  auto state = std::make_unique<ThrottleState<Args...>>();
  state->interval = interval;
  state->last_call_time = std::chrono::steady_clock::time_point{};  // epoch，允许立即首次调用
  return state;
}

// 创建节流状态（无参数版本）
template <>
auto create<void>(std::chrono::milliseconds interval) -> std::unique_ptr<ThrottleState<void>> {
  auto state = std::make_unique<ThrottleState<void>>();
  state->interval = interval;
  state->last_call_time = std::chrono::steady_clock::time_point{};
  return state;
}

// 检查是否可以立即执行（仅检查时间，不修改状态）
template <typename... Args>
auto can_call(const ThrottleState<Args...>& state) -> bool {
  // 注意：这里读取不是线程安全的，仅用于快速检查
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.last_call_time;
  return elapsed >= state.interval;
}

// 重置节流状态
template <typename... Args>
auto reset(ThrottleState<Args...>& state) -> void {
  std::lock_guard lock(state.mutex);
  state.has_pending = false;
  state.last_call_time = std::chrono::steady_clock::time_point{};
}

// 节流调用（带参数版本）
// 返回 true 表示本次调用被执行，false 表示被节流跳过（已缓存参数）
template <typename Func, typename... Args>
auto call(ThrottleState<Args...>& state, Func&& func, Args... args) -> bool {
  std::lock_guard lock(state.mutex);

  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.last_call_time;

  if (elapsed >= state.interval) {
    // 满足间隔，立即执行
    state.last_call_time = now;
    state.has_pending = false;
    std::forward<Func>(func)(args...);
    return true;
  } else {
    // 间隔不足，缓存参数（Leading Edge executed, Trailing Edge scheduled）
    state.has_pending = true;
    state.pending_args = std::make_tuple(args...);
    return false;
  }
}

// 节流调用（无参数版本）
template <typename Func>
auto call(ThrottleState<void>& state, Func&& func) -> bool {
  std::lock_guard lock(state.mutex);

  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.last_call_time;

  if (elapsed >= state.interval) {
    state.last_call_time = now;
    state.has_pending = false;
    std::forward<Func>(func)();
    return true;
  } else {
    state.has_pending = true;
    return false;
  }
}

// 强制执行待处理的调用 (Trailing Edge)
// 如果有 pending 调用，则执行它并清空 pending 标志
// 返回 true 表示执行了 pending 调用，false 表示没有 pending
template <typename Func, typename... Args>
auto flush(ThrottleState<Args...>& state, Func&& func) -> bool {
  std::lock_guard lock(state.mutex);

  if (!state.has_pending) {
    return false;
  }

  // 执行缓存的参数
  std::apply(std::forward<Func>(func), state.pending_args);

  state.has_pending = false;
  state.last_call_time = std::chrono::steady_clock::now();
  return true;
}

// 强制执行待处理的调用（无参数版本）
template <typename Func>
auto flush(ThrottleState<void>& state, Func&& func) -> bool {
  std::lock_guard lock(state.mutex);

  if (!state.has_pending) {
    return false;
  }

  std::forward<Func>(func)();

  state.has_pending = false;
  state.last_call_time = std::chrono::steady_clock::now();
  return true;
}

}  // namespace Utils::Throttle
