module;

module Core.Events;

import std;

namespace Core::Events {

auto process_events(State::EventsState& bus) -> void {
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
        } catch (const std::exception& e) {
          // 异常处理暂时省略，避免循环依赖
        }
      }
    }

    events_to_process.pop();
  }
}

auto clear_events(State::EventsState& bus) -> void {
  std::lock_guard<std::mutex> lock(bus.queue_mutex);
  bus.event_queue = {};  // 清空
}

}  // namespace Core::Events