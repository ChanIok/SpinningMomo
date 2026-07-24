#include "core/notifications/notifications.hpp"

#include "core/events/events.hpp"
#include "core/notifications/events.hpp"
#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "utils/string/string.hpp"

namespace Core::Notifications {

auto show_notification(Core::State::AppState& state, Types::NotificationOptions options) -> void {
  UI::NotificationWindow::show_notification(state, std::move(options));
}

auto post_notification_request(Core::State::AppState& state, Types::NotificationOptions options)
    -> void {
  Core::Events::post(state, Events::NotificationRequestEvent{.options = std::move(options)});
}

// UI 线程、无 action：直接显示，不经过事件队列。
auto show_notification(Core::State::AppState& state, const std::string& title,
                       const std::string& message) -> void {
  Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(title);
  options.message = Utils::String::FromUtf8(message);
  show_notification(state, std::move(options));
}

}  // namespace Core::Notifications
