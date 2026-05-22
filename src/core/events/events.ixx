module;

export module Core.Events;

import std;
import Core.State;
import <windows.h>;

namespace Core::Events {

// Custom message for UI thread wake-up to process async events
export constexpr UINT kWM_APP_PROCESS_EVENTS = WM_APP + 1;

auto send_event(Core::State::AppState& state, std::type_index key, const std::any& data) -> void;

auto post_event(Core::State::AppState& state, std::type_index key, std::any data) -> void;

auto subscribe_event(Core::State::AppState& state, std::type_index key,
                     std::function<void(const std::any&)> handler) -> void;

// 同步发送事件
export template <typename T>
auto send(Core::State::AppState& state, const T& event) -> void {
  send_event(state, std::type_index(typeid(T)), std::any(event));
}

// 异步投递事件
export template <typename T>
auto post(Core::State::AppState& state, T event) -> void {
  post_event(state, std::type_index(typeid(T)), std::any(std::move(event)));
}

// 订阅事件
export template <typename T>
auto subscribe(Core::State::AppState& state, std::function<void(const T&)> handler) -> void {
  if (!handler) {
    return;
  }

  subscribe_event(state, std::type_index(typeid(T)),
                  [handler = std::move(handler)](const std::any& data) {
                    try {
                      handler(std::any_cast<const T&>(data));
                    } catch (const std::bad_any_cast&) {
                      // 类型转换错误，暂时忽略
                    }
                  });
}

// 处理队列中的事件（在消息循环中调用）
export auto process_events(Core::State::AppState& state) -> void;

}  // namespace Core::Events
