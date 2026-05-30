module;

export module Features.Notifications.State;

import std;
import Features.Notifications.Types;
import Vendor.Windows;
import <d2d1_3.h>;
import <dwrite_3.h>;

namespace Features::Notifications::State {

export enum class NotificationHitKind {
  None,
  Content,
  Action,
  Card,
};

export struct NotificationHitTarget {
  NotificationHitKind kind = NotificationHitKind::None;
  size_t notification_id = 0;
};

export struct NotificationRenderSurface {
  ID2D1Factory7* factory = nullptr;
  IDWriteFactory7* write_factory = nullptr;
  ID2D1DCRenderTarget* render_target = nullptr;
  ID2D1DeviceContext6* device_context = nullptr;
  HDC memory_dc = nullptr;
  HBITMAP dib_bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  void* bitmap_bits = nullptr;
  SIZE bitmap_size{};

  IDWriteTextFormat* title_text_format = nullptr;
  IDWriteTextFormat* message_text_format = nullptr;
  IDWriteTextFormat* button_text_format = nullptr;

  bool is_ready = false;
  bool is_rendering = false;
  int dpi = 96;
};

// 单个通知的所有数据
export struct Notification {
  size_t id;  // 唯一ID
  std::wstring title;
  std::wstring message;
  std::optional<Types::NotificationAction> action;

  // 主题颜色快照（创建通知时读取设置，确保通知生命周期内外观稳定）
  Types::NotificationThemeColors colors;

  // 通知当前的动画/生命周期阶段
  Types::NotificationAnimState state = Types::NotificationAnimState::Spawning;

  // 动画和时间
  std::chrono::steady_clock::time_point last_state_change_time;
  std::chrono::steady_clock::time_point display_started_time;
  float opacity = 0.0f;
  float animation_start_opacity = 0.0f;
  Vendor::Windows::POINT animation_start_pos;
  Vendor::Windows::POINT current_pos;
  Vendor::Windows::POINT target_pos;
  int height = 0;
  int width = 0;
  // 通知创建时测量一次，后续只随 current_pos 更新 hit/draw 矩形。
  Types::NotificationLayoutMetrics layout{};
  std::chrono::milliseconds duration{3000};

  Vendor::Windows::RECT card_rect{};
  Vendor::Windows::RECT content_rect{};
  Vendor::Windows::RECT title_rect{};
  Vendor::Windows::RECT message_rect{};
  Vendor::Windows::RECT action_rect{};

  // 鼠标悬停状态
  bool is_hovered = false;
  bool action_hovered = false;
  std::chrono::milliseconds total_paused_duration{0};
  std::chrono::steady_clock::time_point pause_start_time;
};

// 整个通知系统的状态
export struct NotificationSystemState {
  Vendor::Windows::HWND host_hwnd = nullptr;
  NotificationRenderSurface surface;

  // 使用 std::list 能保证元素指针和引用的稳定性，
  // 这对于将指针传递给 WNDPROC 非常重要。
  std::list<Notification> active_notifications;
  size_t next_id = 0;

  Vendor::Windows::SIZE host_size{};
  Vendor::Windows::POINT host_position{};
  int dpi = 96;

  NotificationHitTarget hover_target;
  NotificationHitTarget pressed_target;

  // 动画定时器状态
  bool animation_timer_active = false;
};

}  // namespace Features::Notifications::State
