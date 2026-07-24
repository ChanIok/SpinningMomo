#pragma once

#include <windows.h>

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/notification_window/types.hpp"

namespace UI::NotificationWindow {

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
auto cleanup(Core::State::AppState& state) -> void;

auto show_notification(Core::State::AppState& state,
                       Core::Notifications::Types::NotificationOptions options) -> void;
auto request_repaint(Core::State::AppState& state) -> void;

auto update_host_bounds(Core::State::AppState& state) -> void;
auto relayout_notifications(Core::State::AppState& state, std::chrono::steady_clock::time_point now)
    -> void;
auto hit_test_notifications(Core::State::AppState& state, POINT point) -> NotificationHitTarget;
auto update_hover_state(Core::State::AppState& state, NotificationHitTarget target) -> bool;
auto handle_click_release(Core::State::AppState& state, NotificationHitTarget release_target)
    -> void;
auto update_notifications(Core::State::AppState& state) -> void;

}  // namespace UI::NotificationWindow
