#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/dcomp.hpp"
#include "vendor/windows/dwrite_3.hpp"
#include "vendor/windows/dxgi1_2.hpp"

#include "core/notifications/types.hpp"
#include "ui/composition_animation/animation.hpp"

namespace ui::notification_window {

// 最大可见通知数量
constexpr int MAX_VISIBLE_NOTIFICATIONS = 5;

// 基础尺寸 (96 DPI)
constexpr int BASE_WINDOW_WIDTH = 350;
constexpr int BASE_MIN_HEIGHT = 80;
constexpr int BASE_MAX_HEIGHT = 200;
constexpr int BASE_PADDING = 16;
constexpr int BASE_TITLE_HEIGHT = 18;
constexpr int BASE_TITLE_MESSAGE_GAP = 8;
constexpr int BASE_FONT_SIZE = 13;
constexpr int BASE_TITLE_FONT_SIZE = 14;
constexpr int BASE_CONTENT_PADDING = 18;
constexpr int BASE_ACTION_COLUMN_GAP = 12;
constexpr int BASE_SPACING = 10;
constexpr int BASE_CORNER_RADIUS = 4;
constexpr int BASE_BUTTON_HEIGHT = 28;
constexpr int BASE_BUTTON_WIDTH = 56;
constexpr int BASE_BUTTON_TEXT_PADDING = 12;

// Win11 卡片描边：固定中性灰，不随背景明暗切换
constexpr int BASE_BORDER_GRAY = 128;
constexpr float BASE_BORDER_ALPHA = 0.45f;

// 动画计时
constexpr auto SLIDE_DURATION = std::chrono::milliseconds(200);
constexpr auto FADE_DURATION = std::chrono::milliseconds(200);

// 仅在生命周期截止时间唤醒，不参与逐帧绘制。
constexpr UINT_PTR LIFECYCLE_TIMER_ID = 1001;
// 只在卡片运动时检查输入，不驱动画面帧率。
constexpr UINT_PTR HOVER_TIMER_ID = 1002;
constexpr UINT HOVER_CHECK_INTERVAL_MS = 16;

// 窗口类名
inline const std::wstring NOTIFICATION_WINDOW_CLASS = L"SpinningMomoNotificationHostClass";

enum class NotificationHitKind {
  None,
  Content,
  Action,
  Card,
};

struct NotificationHitTarget {
  NotificationHitKind kind = NotificationHitKind::None;
  size_t notification_id = 0;
};

enum class NotificationPhase { Entering, Visible, Leaving };

struct NotificationLifetime {
  NotificationPhase phase = NotificationPhase::Entering;
  // Entering 的空截止时间表示尚未启动入场。
  std::chrono::steady_clock::time_point deadline{};
  std::chrono::steady_clock::duration remaining_display_time{};
};

struct NotificationMotion {
  ui::composition_animation::Transition offset_x;
  ui::composition_animation::Transition offset_y;
  ui::composition_animation::Transition opacity;
  // 运动结束后重新判断静止鼠标的悬停，不参与展示计时。
  std::chrono::steady_clock::time_point deadline{};
  bool dirty = true;
};

struct NotificationThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F hover{};
};

// 卡片局部布局；绘制和命中测试共用，位移单独由 visual 承担。
struct NotificationLayoutMetrics {
  int padding = 0;
  int content_padding = 0;
  int column_gap = 0;
  int title_height = 0;
  int title_message_gap = 0;
  int button_height = 0;
  int content_height = 0;
  int action_width = 0;
};

struct Notification {
  size_t id = 0;
  std::wstring title;
  std::wstring message;
  std::optional<core::notifications::NotificationAction> action;

  // 主题颜色快照（创建通知时读取设置，确保通知生命周期内外观稳定）
  NotificationThemeColors colors;

  NotificationLifetime lifetime;
  NotificationMotion motion;
  D2D1_POINT_2F layout_target{};

  wil::com_ptr<IDCompositionVisual> visual;
  wil::com_ptr<IDCompositionEffectGroup> opacity_effect;
  wil::com_ptr<IDCompositionSurface> surface;
  bool content_dirty = true;
  int surface_padding = 0;
  int dpi = 0;
  int height = 0;
  int width = 0;
  NotificationLayoutMetrics layout{};
  std::chrono::milliseconds duration{3000};

  RECT card_rect{};
  RECT content_rect{};
  RECT title_rect{};
  RECT message_rect{};
  RECT action_rect{};

  // 鼠标悬停状态
  bool is_hovered = false;
  bool action_hovered = false;
};

struct RenderResources {
  wil::com_ptr<IDCompositionDevice> composition_device;
  wil::com_ptr<IDCompositionTarget> composition_target;
  wil::com_ptr<IDCompositionVisual> composition_visual;

  wil::com_ptr<ID2D1DeviceContext6> device_context;

  // 通知窗口使用共享设备级资源，但保留自己的文本格式和画刷缓存。
  wil::com_ptr<IDWriteTextFormat> title_text_format;
  wil::com_ptr<IDWriteTextFormat> message_text_format;
  wil::com_ptr<IDWriteTextFormat> button_text_format;
  wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
  wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
  wil::com_ptr<ID2D1SolidColorBrush> text_brush;

  bool is_ready = false;
  bool is_rendering = false;
  int dpi = 96;
};

}  // namespace ui::notification_window
