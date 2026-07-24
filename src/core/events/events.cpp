#include "core/events/events.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/events/state.hpp"
#include "core/state/app_state.hpp"

namespace core::events {

// 同步分发一个事件：按类型查找订阅者并依次调用
auto send_event(core::AppState& state, std::type_index key, const std::any& data) -> void {
  if (!state.events) {
    return;
  }

  auto& bus = *state.events;
  if (auto it = bus.handlers.find(key); it != bus.handlers.end()) {
    for (const auto& handler : it->second) {
      try {
        handler(data);
      } catch (const std::exception&) {
        // 异常处理暂时省略，避免循环依赖
      }
    }
  }
}

// 异步投递一个事件：入队后唤醒 UI 消息循环
auto post_event(core::AppState& state, std::type_index key, std::any data) -> void {
  if (!state.events) {
    return;
  }

  auto& bus = *state.events;
  {
    std::lock_guard<std::mutex> lock(bus.queue_mutex);
    bus.event_queue.emplace(key, std::move(data));
  }

  if (bus.notify_hwnd) {
    ::PostMessageW(bus.notify_hwnd, kWM_APP_PROCESS_EVENTS, 0, 0);
  }
}

// 注册类型擦除后的事件处理器，由总线独占并重复调用
auto subscribe_event(core::AppState& state, std::type_index key,
                     std::move_only_function<void(const std::any&) const> handler) -> void {
  if (!state.events || !handler) {
    return;
  }

  // 总线成为处理器的唯一所有者，后续事件只通过引用重复调用
  auto& bus = *state.events;
  bus.handlers[key].emplace_back(std::move(handler));
}

// 批量处理异步事件：先缩短持锁时间，再在 UI 线程完成分发
auto process_events_executor(EventsState& bus) -> void {
  std::queue<std::pair<std::type_index, std::any>> events_to_process;

  // 快速获取事件队列的副本，减少锁的持有时间
  {
    std::lock_guard<std::mutex> lock(bus.queue_mutex);
    if (bus.event_queue.empty()) {
      return;
    }
    events_to_process.swap(bus.event_queue);
  }

  // 处理所有事件
  while (!events_to_process.empty()) {
    const auto& [type_index, event_data] = events_to_process.front();

    // 查找并调用对应类型的处理器
    if (auto it = bus.handlers.find(type_index); it != bus.handlers.end()) {
      for (const auto& handler : it->second) {
        try {
          handler(event_data);
        } catch (const std::exception&) {
          // 异常处理暂时省略，避免循环依赖
        }
      }
    }

    events_to_process.pop();
  }
}

// 处理当前待分发的异步事件
auto process_events(core::AppState& state) -> void {
  if (!state.events) {
    return;
  }

  auto& bus = *state.events;
  process_events_executor(bus);
}

}  // namespace core::events
