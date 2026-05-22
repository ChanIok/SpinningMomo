module;

module Core.Events;

import std;
import Core.Events.State;
import Core.State;
import <windows.h>;

namespace Core::Events {

auto send_event(Core::State::AppState& state, std::type_index key, const std::any& data) -> void {
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

auto post_event(Core::State::AppState& state, std::type_index key, std::any data) -> void {
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

auto subscribe_event(Core::State::AppState& state, std::type_index key,
                     std::function<void(const std::any&)> handler) -> void {
  if (!state.events || !handler) {
    return;
  }

  auto& bus = *state.events;
  bus.handlers[key].emplace_back(std::move(handler));
}

auto process_events_executor(State::EventsState& bus) -> void {
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

auto process_events(Core::State::AppState& state) -> void {
  if (!state.events) {
    return;
  }

  auto& bus = *state.events;
  process_events_executor(bus);
}

}  // namespace Core::Events
