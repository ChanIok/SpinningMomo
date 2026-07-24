#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "ui/notification_window/types.hpp"

namespace ui::notification_window {

struct NotificationWindowState {
  HWND host_hwnd = nullptr;
  notification_window::RenderResources render_resources;

  // 使用 std::list 保持元素地址稳定，方便动画与命中测试按 id 回查。
  std::list<notification_window::Notification> active_notifications;
  size_t next_id = 0;

  SIZE host_size{};
  POINT host_position{};
  int dpi = 96;

  notification_window::NotificationHitTarget hover_target;
  notification_window::NotificationHitTarget pressed_target;

  bool animation_timer_active = false;
};

}  // namespace ui::notification_window
