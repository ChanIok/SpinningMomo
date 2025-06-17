module;

module Core.Events;

import std;

namespace Core::Events {

auto EventDispatcher::Subscribe(EventType type, EventHandler handler) -> void {
  if (handler) {
    m_handlers[type].emplace_back(std::move(handler));
  }
}

auto EventDispatcher::Subscribe(EventType type, std::shared_ptr<IEventHandler> handler) -> void {
  if (handler) {
    m_interface_handlers[type].emplace_back(handler);
  }
}

auto EventDispatcher::SendEvent(Event event) -> void {
  CallHandlers(event);
}

auto EventDispatcher::PostEvent(Event event) -> void {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  m_event_queue.emplace(std::move(event));
}

auto EventDispatcher::ProcessEvents() -> void {
  std::queue<Event> events_to_process;
  
  // 快速获取事件队列的副本，减少锁的持有时间
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    events_to_process = std::move(m_event_queue);
    m_event_queue = std::queue<Event>{};  // 清空原队列
  }

  // 处理所有事件
  while (!events_to_process.empty()) {
    auto& event = events_to_process.front();
    CallHandlers(event);
    events_to_process.pop();
  }
}

auto EventDispatcher::ClearEvents() -> void {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  m_event_queue = std::queue<Event>{};
}

auto EventDispatcher::CallHandlers(const Event& event) -> void {
  // 调用函数回调处理器
  if (auto it = m_handlers.find(event.type); it != m_handlers.end()) {
    for (const auto& handler : it->second) {
      try {
        handler(event);
      } catch (const std::exception& e) {
        // 这里可以添加日志记录，但为了避免循环依赖暂时省略
        // Logger().error("Event handler exception: {}", e.what());
      }
    }
  }

  // 调用接口回调处理器
  if (auto it = m_interface_handlers.find(event.type); it != m_interface_handlers.end()) {
    // 清理失效的弱引用
    auto& handlers = it->second;
    handlers.erase(
        std::remove_if(handlers.begin(), handlers.end(),
                       [](const std::weak_ptr<IEventHandler>& weak_handler) {
                         return weak_handler.expired();
                       }),
        handlers.end());

    // 调用有效的处理器
    for (const auto& weak_handler : handlers) {
      if (auto handler = weak_handler.lock()) {
        try {
          handler->HandleEvent(event);
        } catch (const std::exception& e) {
          // 同样，这里可以添加日志记录
        }
      }
    }
  }
}

}  // namespace Core::Events 