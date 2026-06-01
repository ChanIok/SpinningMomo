module;

export module UI.NotificationWindow;

import std;
import Core.State;
import Core.Notifications.Types;
import UI.NotificationWindow.Types;
import <windows.h>;

namespace UI::NotificationWindow {

export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto cleanup(Core::State::AppState& state) -> void;

export auto show_notification(Core::State::AppState& state,
                              Core::Notifications::Types::NotificationOptions options) -> void;
export auto request_repaint(Core::State::AppState& state) -> void;

export auto update_host_bounds(Core::State::AppState& state) -> void;
export auto relayout_notifications(Core::State::AppState& state,
                                   std::chrono::steady_clock::time_point now) -> void;
export auto hit_test_notifications(Core::State::AppState& state, POINT point)
    -> NotificationHitTarget;
export auto update_hover_state(Core::State::AppState& state, NotificationHitTarget target) -> bool;
export auto handle_click_release(Core::State::AppState& state, NotificationHitTarget release_target)
    -> void;
export auto update_notifications(Core::State::AppState& state) -> void;

}  // namespace UI::NotificationWindow
