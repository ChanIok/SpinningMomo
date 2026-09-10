#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/notification_window/types.hpp"

namespace ui::notification_window::painter {

auto scale_for_dpi(int value, int dpi) -> int;
auto get_current_dpi(const core::AppState& state) -> int;
auto get_window_width(int dpi) -> int;
auto get_layout_margin(int dpi) -> int;
auto get_surface_padding(int dpi) -> int;
auto get_host_size(int dpi) -> SIZE;

auto resolve_notification_theme_colors(const core::AppState& state)
    -> notification_window::NotificationThemeColors;
auto normalize_action(std::optional<core::notifications::NotificationAction> action)
    -> std::optional<core::notifications::NotificationAction>;
auto compute_notification_layout(
    core::AppState& state, const std::wstring& message,
    const std::optional<core::notifications::NotificationAction>& action, int card_width)
    -> notification_window::NotificationLayoutMetrics;
auto measure_card_height(const notification_window::NotificationLayoutMetrics& layout, int dpi)
    -> int;

auto update_all_notification_rects(core::AppState& state) -> void;

// 绘制单张卡片内容到其独立的 DComp 表面
auto paint_card(core::AppState& state, Notification& notification) -> bool;

auto request_repaint(core::AppState& state) -> void;

}  // namespace ui::notification_window::painter
