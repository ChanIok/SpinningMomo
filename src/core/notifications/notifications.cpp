#include "core/notifications/notifications.hpp"

#include "vendor/std.hpp"

#include "core/events/events.hpp"
#include "core/notifications/events.hpp"
#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "utils/string/string.hpp"

namespace core::notifications {

auto show_notification(core::AppState& state, NotificationOptions options) -> void {
  ui::notification_window::show_notification(state, std::move(options));
}

auto post_notification_request(core::AppState& state, NotificationOptions options) -> void {
  core::events::post(state, events::NotificationRequestEvent{.options = std::move(options)});
}

// UI 线程、无 action：直接显示，不经过事件队列。
auto show_notification(core::AppState& state, const std::string& title, const std::string& message)
    -> void {
  NotificationOptions options;
  options.title = utils::string::FromUtf8(title);
  options.message = utils::string::FromUtf8(message);
  show_notification(state, std::move(options));
}

}  // namespace core::notifications
