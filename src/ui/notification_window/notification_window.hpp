#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/notification_window/types.hpp"

namespace ui::notification_window {

auto initialize(core::AppState& state) -> std::expected<void, std::string>;
auto cleanup(core::AppState& state) -> void;

auto show_notification(core::AppState& state, core::notifications::NotificationOptions options)
    -> void;
auto request_repaint(core::AppState& state) -> void;

auto update_host_bounds(core::AppState& state) -> void;
auto relayout_notifications(core::AppState& state, std::chrono::steady_clock::time_point now)
    -> void;
auto hit_test_notifications(core::AppState& state, POINT point) -> NotificationHitTarget;
auto update_hover_state(core::AppState& state, NotificationHitTarget target) -> bool;
auto handle_click_release(core::AppState& state, NotificationHitTarget release_target) -> void;
auto update_notifications(core::AppState& state) -> void;

}  // namespace ui::notification_window
