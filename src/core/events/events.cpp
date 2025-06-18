module;

module Core.Events;

import std;

namespace Core::Events {

// 内部处理器调用
auto call_handlers(EventBus& bus, const Event& event) -> void {
  // 调用函数回调处理器
  if (auto it = bus.handlers.find(event.type); it != bus.handlers.end()) {
    for (const auto& handler : it->second) {
      try {
        handler(event);
      } catch (const std::exception& e) {
        // 这里可以添加日志记录，但为了避免循环依赖暂时省略
        // Logger().error("Event handler exception: {}", e.what());
      }
    }
  }
}

auto subscribe(EventBus& bus, EventType type, EventHandler handler) -> void {
  if (handler) {
    bus.handlers[type].emplace_back(std::move(handler));
  }
}

auto send_event(EventBus& bus, const Event& event) -> void { call_handlers(bus, event); }

auto post_event(EventBus& bus, Event event) -> void {
  std::lock_guard<std::mutex> lock(bus.queue_mutex);
  bus.event_queue.emplace(std::move(event));
}

auto process_events(EventBus& bus) -> void {
  std::queue<Event> events_to_process;

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
    const auto& event = events_to_process.front();
    call_handlers(bus, event);
    events_to_process.pop();
  }
}

auto clear_events(EventBus& bus) -> void {
  std::lock_guard<std::mutex> lock(bus.queue_mutex);
  bus.event_queue = {};  // 清空
}

}  // namespace Core::Events 