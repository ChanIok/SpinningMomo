module;

export module Core.Notifications;

import std;
import Core.State;
import Core.Notifications.Types;

namespace Core::Notifications {

// UI 线程、无 action 的简单 toast（i18n UTF-8 文本）
export auto show_notification(Core::State::AppState& state, const std::string& title,
                              const std::string& message) -> void;

// 非 UI 线程，或需要 action / 自定义 duration 时：post 后在 UI 线程显示
export auto post_notification_request(Core::State::AppState& state,
                                      Types::NotificationOptions options) -> void;

// 由事件订阅方调用；业务代码请用 show_notification 或 post_notification_request
export auto show_notification(Core::State::AppState& state, Types::NotificationOptions options)
    -> void;

}  // namespace Core::Notifications
