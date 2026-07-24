#pragma once

#include "vendor/std.hpp"

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"

namespace core::notifications {

// UI 线程、无 action 的简单 toast（i18n UTF-8 文本）
auto show_notification(core::AppState& state, const std::string& title, const std::string& message)
    -> void;

// 非 UI 线程，或需要 action / 自定义 duration 时：post 后在 UI 线程显示
auto post_notification_request(core::AppState& state, NotificationOptions options) -> void;

// 由事件订阅方调用；业务代码请用 show_notification 或 post_notification_request
auto show_notification(core::AppState& state, NotificationOptions options) -> void;

}  // namespace core::notifications
