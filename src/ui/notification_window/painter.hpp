#pragma once

#include <windows.h>

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/notification_window/types.hpp"

namespace UI::NotificationWindow::Painter {

auto scale_for_dpi(int value, int dpi) -> int;
auto is_exiting(NotificationWindow::NotificationAnimState state) -> bool;
auto get_current_dpi(const Core::State::AppState& state) -> int;
auto get_window_width(int dpi) -> int;
auto get_layout_margin(int dpi) -> int;
auto get_host_size(int dpi) -> SIZE;

auto resolve_notification_theme_colors(const Core::State::AppState& state)
    -> NotificationWindow::NotificationThemeColors;
auto normalize_action(std::optional<Core::Notifications::Types::NotificationAction> action)
    -> std::optional<Core::Notifications::Types::NotificationAction>;
auto compute_notification_layout(
    Core::State::AppState& state, const std::wstring& message,
    const std::optional<Core::Notifications::Types::NotificationAction>& action, int card_width)
    -> NotificationWindow::NotificationLayoutMetrics;
auto measure_card_height(const NotificationWindow::NotificationLayoutMetrics& layout, int dpi)
    -> int;

auto update_all_notification_rects(Core::State::AppState& state) -> void;
auto paint_notifications(Core::State::AppState& state) -> void;
auto request_repaint(Core::State::AppState& state) -> void;

}  // namespace UI::NotificationWindow::Painter
