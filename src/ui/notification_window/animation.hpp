#pragma once

#include "vendor/std.hpp"
#include "vendor/windows.hpp"

#include "ui/notification_window/types.hpp"

namespace ui::notification_window::animation {

// 一次更新共用时间快照，合成曲线与 CPU 命中测试使用相同的 QPC 时间轴。
struct UpdateTime {
  LARGE_INTEGER motion;
  std::chrono::steady_clock::time_point lifetime;
};

// 获取当前时刻的原子时间快照（QPC 与 steady_clock）
auto now() -> UpdateTime;

// 采样卡片在指定 QPC 时刻的 X/Y 屏幕相对偏移坐标
auto sample_position(const NotificationMotion& motion, LARGE_INTEGER time) -> D2D1_POINT_2F;

// 启动卡片入场动画（滑入与淡入）
auto enter(NotificationMotion& motion, D2D1_POINT_2F origin, D2D1_POINT_2F target,
           const UpdateTime& time) -> void;

// 启动卡片重排避让动画（X/Y 平滑位移）
auto move_to(NotificationMotion& motion, D2D1_POINT_2F target, const UpdateTime& time) -> void;

// 启动卡片离场动画（原位淡出）
auto leave(NotificationMotion& motion, const UpdateTime& time) -> void;

}  // namespace ui::notification_window::animation
