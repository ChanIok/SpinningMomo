#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace core::events {

// Custom message for UI thread wake-up to process async events
constexpr UINT kWM_APP_PROCESS_EVENTS = WM_APP + 1;

auto send_event(core::AppState& state, std::type_index key, const std::any& data) -> void;

auto post_event(core::AppState& state, std::type_index key, std::any data) -> void;

auto subscribe_event(core::AppState& state, std::type_index key,
                     std::move_only_function<void(const std::any&) const> handler) -> void;

// 同步发送事件
template <typename T>
inline auto send(core::AppState& state, const T& event) -> void {
  send_event(state, std::type_index(typeid(T)), std::any(event));
}

// 异步投递事件
template <typename T>
inline auto post(core::AppState& state, T event) -> void {
  post_event(state, std::type_index(typeid(T)), std::any(std::move(event)));
}

// 订阅强类型事件：把处理器移动进总线并在类型擦除边界完成 any_cast
template <typename T>
inline auto subscribe(core::AppState& state, std::move_only_function<void(const T&) const> handler)
    -> void {
  if (!handler) {
    return;
  }

  subscribe_event(state, std::type_index(typeid(T)),
                  [handler = std::move(handler)](const std::any& data) {
                    handler(std::any_cast<const T&>(data));
                  });
}

// 处理队列中的事件（在消息循环中调用）
auto process_events(core::AppState& state) -> void;

}  // namespace core::events
