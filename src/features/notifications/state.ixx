module;

export module Features.Notifications.State;

import std;
import Vendor.Windows;

namespace Features::Notifications::State {

// 通知当前的动画/生命周期阶段
export enum class NotificationAnimState {
  Spawning,    // 正在生成，尚未创建窗口
  SlidingIn,   // 滑入动画
  Displaying,  // 正常显示
  MovingUp,    // 为新通知腾出空间而上移
  FadingOut,   // 淡出动画
  Done         // 处理完毕，待销毁
};

// 单个通知的所有数据
export struct Notification {
  size_t id;  // 唯一ID
  std::wstring title;
  std::wstring message;
  // 未来可以扩展: NotificationType type;

  // 主题颜色快照（创建通知时读取设置，确保通知生命周期内外观稳定）
  std::uint32_t bg_color = 0;
  std::uint32_t text_color = 0;
  std::uint32_t title_color = 0;
  std::uint32_t close_normal_color = 0;
  std::uint32_t close_hover_color = 0;

  NotificationAnimState state = NotificationAnimState::Spawning;
  Vendor::Windows::HWND hwnd = nullptr;

  // 动画和时间
  std::chrono::steady_clock::time_point last_state_change_time;
  float opacity = 0.0f;
  Vendor::Windows::POINT current_pos;
  Vendor::Windows::POINT target_pos;
  int height = 0;

  // 鼠标悬停状态
  bool is_hovered = false;
  bool is_close_hovered = false;
  std::chrono::milliseconds total_paused_duration{0};
  std::chrono::steady_clock::time_point pause_start_time;
};

// 整个通知系统的状态
export struct NotificationSystemState {
  // 使用 std::list 能保证元素指针和引用的稳定性，
  // 这对于将指针传递给 WNDPROC 非常重要。
  std::list<Notification> active_notifications;
  size_t next_id = 0;

  // 动画定时器状态
  bool animation_timer_active = false;
};

}  // namespace Features::Notifications::State
