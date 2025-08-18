module;

export module Core.Events;

import std;

namespace Core::Events {

// 事件总线（纯数据结构）
export struct EventBus {
  std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> handlers;
  std::queue<std::pair<std::type_index, std::any>> event_queue;
  std::mutex queue_mutex;
};

// 同步发送事件
export template <typename T>
auto send(EventBus& bus, const T& event) -> void {
  auto key = std::type_index(typeid(T));
  if (auto it = bus.handlers.find(key); it != bus.handlers.end()) {
    for (const auto& handler : it->second) {
      try {
        handler(std::any(event));
      } catch (const std::exception& e) {
        // 异常处理暂时省略，避免循环依赖
      }
    }
  }
}

// 异步投递事件
export template <typename T>
auto post(EventBus& bus, T event) -> void {
  std::lock_guard<std::mutex> lock(bus.queue_mutex);
  auto key = std::type_index(typeid(T));
  bus.event_queue.emplace(key, std::any(std::move(event)));
}

// 订阅事件
export template <typename T>
auto subscribe(EventBus& bus, std::function<void(const T&)> handler) -> void {
  if (handler) {
    auto key = std::type_index(typeid(T));
    bus.handlers[key].emplace_back([handler = std::move(handler)](const std::any& data) {
      try {
        const T& event = std::any_cast<const T&>(data);
        handler(event);
      } catch (const std::bad_any_cast& e) {
        // 类型转换错误，暂时忽略
      }
    });
  }
}

// 处理队列中的事件（在消息循环中调用）
export auto process_events(EventBus& bus) -> void;

// 清空事件队列
export auto clear_events(EventBus& bus) -> void;

}  // namespace Core::Events