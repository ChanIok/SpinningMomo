module;

export module UI.NotificationWindow.State;

import std;
import UI.NotificationWindow.Types;
import <windows.h>;

namespace UI::NotificationWindow::State {

export struct NotificationWindowState {
  HWND host_hwnd = nullptr;
  NotificationWindow::RenderResources render_resources;

  // 使用 std::list 保持元素地址稳定，方便动画与命中测试按 id 回查。
  std::list<NotificationWindow::Notification> active_notifications;
  size_t next_id = 0;

  SIZE host_size{};
  POINT host_position{};
  int dpi = 96;

  NotificationWindow::NotificationHitTarget hover_target;
  NotificationWindow::NotificationHitTarget pressed_target;

  bool animation_timer_active = false;
};

}  // namespace UI::NotificationWindow::State
