#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/dcomp.hpp"
#include "vendor/windows/dwrite_3.hpp"
#include "vendor/windows/dxgi1_2.hpp"

#include "core/notifications/types.hpp"

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

// 动画定时器
constexpr UINT_PTR ANIMATION_TIMER_ID = 1001;
constexpr UINT ANIMATION_FRAME_INTERVAL = 16;  // ~60fps

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

// 通知当前的动画/生命周期阶段
enum class NotificationAnimState {
  Spawning,    // 正在生成，尚未进入动画
  SlidingIn,   // 滑入动画
  Displaying,  // 正常显示
  MovingUp,    // 为新通知腾出空间而上移
  FadingOut,   // 淡出动画
  Done         // 处理完毕，待销毁
};

struct NotificationThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F hover{};
};

// 通知创建时测量一次，后续只随 current_pos 更新 hit/draw 矩形。
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

  // 通知当前的动画/生命周期阶段
  NotificationAnimState state = NotificationAnimState::Spawning;

  // 动画和时间
  std::chrono::steady_clock::time_point last_state_change_time;
  std::chrono::steady_clock::time_point display_started_time;
  float opacity = 0.0f;
  float animation_start_opacity = 0.0f;
  POINT animation_start_pos{};
  POINT current_pos{};
  POINT target_pos{};
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
  std::chrono::milliseconds total_paused_duration{0};
  std::chrono::steady_clock::time_point pause_start_time;
};

struct RenderResources {
  wil::com_ptr<IDXGISwapChain1> swap_chain;
  wil::com_ptr<IDCompositionTarget> composition_target;
  wil::com_ptr<IDCompositionVisual> composition_visual;

  wil::com_ptr<ID2D1DeviceContext6> device_context;
  wil::com_ptr<ID2D1Bitmap1> target_bitmap;

  // 当前 back buffer 尺寸缓存；resize 只在尺寸变化时重建目标位图。
  SIZE surface_size = {0, 0};

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
