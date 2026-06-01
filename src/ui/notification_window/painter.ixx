module;

export module UI.NotificationWindow.Painter;

import std;
import Core.State;
import Core.Notifications.Types;
import UI.NotificationWindow.Types;
import <windows.h>;

namespace UI::NotificationWindow::Painter {

export auto scale_for_dpi(int value, int dpi) -> int;
export auto is_exiting(NotificationWindow::NotificationAnimState state) -> bool;
export auto get_current_dpi(const Core::State::AppState& state) -> int;
export auto get_window_width(int dpi) -> int;
export auto get_layout_margin(int dpi) -> int;
export auto get_host_size(int dpi) -> SIZE;

export auto resolve_notification_theme_colors(const Core::State::AppState& state)
    -> NotificationWindow::NotificationThemeColors;
export auto normalize_action(std::optional<Core::Notifications::Types::NotificationAction> action)
    -> std::optional<Core::Notifications::Types::NotificationAction>;
export auto compute_notification_layout(
    Core::State::AppState& state, const std::wstring& message,
    const std::optional<Core::Notifications::Types::NotificationAction>& action, int card_width)
    -> NotificationWindow::NotificationLayoutMetrics;
export auto measure_card_height(const NotificationWindow::NotificationLayoutMetrics& layout,
                                int dpi) -> int;

export auto update_all_notification_rects(Core::State::AppState& state) -> void;
export auto paint_notifications(Core::State::AppState& state) -> void;
export auto request_repaint(Core::State::AppState& state) -> void;

}  // namespace UI::NotificationWindow::Painter
