module;

// 通知的 D2D 绘制与布局测量。
// toast 不是独立 HWND，而是画在透明宿主窗口上；本模块负责 surface 生命周期、
// 文本/按钮尺寸计算，以及每帧整组通知的重绘。
export module Features.Notifications.Draw;

import std;
import Core.State;
import Features.Notifications.Types;
import Vendor.Windows;

namespace Features::Notifications {

// DPI 与宿主画布尺寸（右下角透明窗口预留阴影边距）
export auto scale_for_dpi(int value, int dpi) -> int;
export auto is_exiting(Types::NotificationAnimState state) -> bool;
export auto get_current_dpi(const Core::State::AppState& state) -> int;
export auto get_window_width(int dpi) -> int;
export auto get_layout_margin(int dpi) -> int;
export auto get_host_size(int dpi) -> Vendor::Windows::SIZE;

// 创建通知时读取主题色、测量正文/action 布局；高度在 layout 基础上 clamp
export auto resolve_notification_theme_colors(const Core::State::AppState& state)
    -> Types::NotificationThemeColors;
export auto normalize_action(std::optional<Types::NotificationAction> action)
    -> std::optional<Types::NotificationAction>;
export auto compute_notification_layout(Core::State::AppState& state, const std::wstring& message,
                                        const std::optional<Types::NotificationAction>& action,
                                        int card_width) -> Types::NotificationLayoutMetrics;
export auto measure_card_height(const Types::NotificationLayoutMetrics& layout, int dpi) -> int;

// D2D + DIB render surface；rect 仅依赖缓存 layout 与 current_pos，绘制前批量更新
export auto ensure_render_surface(Core::State::AppState& state) -> bool;
export auto cleanup_render_surface(Core::State::AppState& state) -> void;
export auto update_all_notification_rects(Core::State::AppState& state) -> void;
export auto paint_notifications(Core::State::AppState& state) -> void;
export auto request_repaint(Core::State::AppState& state) -> void;

}  // namespace Features::Notifications
