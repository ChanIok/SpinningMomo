#include "ui/notification_window/animation.hpp"

#include "vendor/std.hpp"

#include "ui/composition_animation/animation.hpp"

namespace ui::notification_window::animation {

namespace curves = ui::composition_animation;

// 获取当前时刻的原子时间快照：同步捕获硬件运动 QPC 与业务生命周期 steady_clock
auto now() -> UpdateTime {
  return {.motion = curves::now(), .lifetime = std::chrono::steady_clock::now()};
}

// 重定向动画目标：采样当前物理时刻的值作为新起点 → 更新目标值与起始时间 → 重置持续时长
auto retarget(curves::Transition& transition, float target, LARGE_INTEGER time,
              std::chrono::milliseconds duration) -> void {
  // 采样当前物理时刻的值作为过渡起点，保证重定向平滑无跳变
  transition = {.from = curves::sample(transition, time),
                .to = target,
                .begin = time,
                .seconds = std::chrono::duration<double>(duration).count()};
}

// 采样通知卡片在当前 QPC 时刻的 X/Y 屏幕相对偏移坐标
auto sample_position(const NotificationMotion& motion, LARGE_INTEGER time) -> D2D1_POINT_2F {
  return {curves::sample(motion.offset_x, time), curves::sample(motion.offset_y, time)};
}

// 启动卡片入场动画：设定起始屏幕外位置 → 触发滑入与淡入过渡 → 设定运动截止时间
auto enter(NotificationMotion& motion, D2D1_POINT_2F origin, D2D1_POINT_2F target,
           const UpdateTime& time) -> void {
  motion.offset_x = {.to = origin.x};
  motion.offset_y = {.to = origin.y};
  motion.opacity = {};
  // 启动 X/Y 轴滑入与透明度淡入
  retarget(motion.offset_x, target.x, time.motion, SLIDE_DURATION);
  retarget(motion.offset_y, target.y, time.motion, SLIDE_DURATION);
  retarget(motion.opacity, 1.0f, time.motion, SLIDE_DURATION);
  // 记录运动截止时间点并标记脏状态
  motion.deadline = time.lifetime + SLIDE_DURATION;
  motion.dirty = true;
}

// 启动卡片重排避让动画：检查目标位置是否变更 → 重定向 X/Y 位移 → 设定运动截止时间
auto move_to(NotificationMotion& motion, D2D1_POINT_2F target, const UpdateTime& time) -> void {
  // 目标位置未变更则忽略
  if (motion.offset_x.to == target.x && motion.offset_y.to == target.y) {
    return;
  }
  // 重定向位移动画到新坐标
  retarget(motion.offset_x, target.x, time.motion, SLIDE_DURATION);
  retarget(motion.offset_y, target.y, time.motion, SLIDE_DURATION);
  motion.deadline = time.lifetime + SLIDE_DURATION;
  motion.dirty = true;
}

// 启动卡片离场动画：锁定当前物理坐标 → 触发淡出过渡 → 清除运动截止时间
auto leave(NotificationMotion& motion, const UpdateTime& time) -> void {
  // 锁定当前瞬时采样位置，原地淡出
  const auto position = sample_position(motion, time.motion);
  motion.offset_x = {.to = position.x};
  motion.offset_y = {.to = position.y};
  retarget(motion.opacity, 0.0f, time.motion, FADE_DURATION);
  motion.deadline = {};
  motion.dirty = true;
}

}  // namespace ui::notification_window::animation
