#include "ui/notification_window/notification_window.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"

#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/animation.hpp"
#include "ui/notification_window/painter.hpp"
#include "ui/notification_window/render_context.hpp"
#include "ui/notification_window/state.hpp"
#include "ui/notification_window/types.hpp"
#include "utils/display/display.hpp"
#include "utils/logger/logger.hpp"

namespace ui::notification_window::message_handler {
LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
}

namespace ui::notification_window {

auto update_at(core::AppState& state, const animation::UpdateTime& time) -> void;

auto hit_targets_equal(const NotificationHitTarget& left, const NotificationHitTarget& right)
    -> bool {
  return left.kind == right.kind && left.notification_id == right.notification_id;
}

auto register_host_window_class(HINSTANCE instance) -> bool {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = message_handler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = notification_window::NOTIFICATION_WINDOW_CLASS.c_str();
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  if (!RegisterClassExW(&wc)) {
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  registered = true;
  return true;
}

auto get_notification_work_area(const core::AppState& state) -> RECT {
  const auto& floating_window = *state.floating_window;
  auto monitor = utils::display::get_working_monitor(floating_window.window.hwnd,
                                                     floating_window.window.is_visible);
  if (monitor) {
    return monitor->work_rect;
  }

  Logger().warn(
      "Failed to resolve notification work area from floating window ({}), fallback to primary",
      monitor.error());
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  return work_area;
}

// 确保通知宿主窗口已创建：注册窗口类 → 测算工作区与屏幕右下角位置 → 创建无重定向顶层窗口
auto ensure_host_window(core::AppState& state) -> bool {
  auto& window_state = *state.notification_window;
  if (window_state.host_hwnd) {
    update_host_bounds(state);
    return true;
  }

  HINSTANCE instance = state.floating_window->window.instance;
  if (!instance) {
    instance = GetModuleHandleW(nullptr);
  }

  if (!register_host_window_class(instance)) {
    return false;
  }

  // 计算 DPI 与宿主窗口尺寸及屏幕右下角停靠坐标
  const int dpi = painter::get_current_dpi(state);
  window_state.layout_dirty = window_state.layout_dirty || window_state.dpi != dpi;
  window_state.dpi = dpi;
  const SIZE host_size = painter::get_host_size(window_state.dpi);
  const RECT work_area = get_notification_work_area(state);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  // 创建全透明、置顶、无重定向位图的通知宿主窗口
  HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              notification_window::NOTIFICATION_WINDOW_CLASS.c_str(),
                              L"SpinningMomoNotifications", WS_POPUP | WS_CLIPCHILDREN,
                              host_position.x, host_position.y, host_size.cx, host_size.cy, nullptr,
                              nullptr, instance, &state);
  if (!hwnd) {
    Logger().error("Failed to create notification host window. Error: {}", GetLastError());
    return false;
  }

  window_state.host_hwnd = hwnd;
  window_state.host_size = host_size;
  window_state.host_position = host_position;
  return true;
}

// 停止生命周期定时器并清空已调度的截止时间
auto stop_lifecycle_timer(core::AppState& state) -> void {
  auto& window = *state.notification_window;
  if (window.host_hwnd && window.scheduled_deadline) {
    KillTimer(window.host_hwnd, LIFECYCLE_TIMER_ID);
  }
  window.scheduled_deadline.reset();
}

// 调度下一个生命周期定时器：寻找所有卡片的最近到期时间点 → 计算相对毫秒延时 → 启动单次 Win32 Timer
auto schedule_lifecycle_timer(core::AppState& state) -> void {
  if (!state.notification_window->host_hwnd) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  auto next = std::chrono::steady_clock::time_point::max();
  // 遍历所有卡片，找出下一个最近到达的生命周期或运动截止点
  for (const auto& notification : state.notification_window->active_notifications) {
    if (notification.lifetime.deadline != std::chrono::steady_clock::time_point{} &&
        !(notification.lifetime.phase == NotificationPhase::Visible && notification.is_hovered)) {
      next = std::min(next, notification.lifetime.deadline);
    }
    if (notification.motion.deadline != std::chrono::steady_clock::time_point{}) {
      next = std::min(next, notification.motion.deadline);
    }
  }
  auto& window = *state.notification_window;
  // 无任何待触发截止时间时直接关闭定时器
  if (next == std::chrono::steady_clock::time_point::max()) {
    stop_lifecycle_timer(state);
    return;
  }
  // 截止点未变时不重复设置定时器
  if (window.scheduled_deadline == next) {
    return;
  }
  stop_lifecycle_timer(state);
  // 计算下一次唤醒所需的相对毫秒延时
  const auto delay = std::chrono::ceil<std::chrono::milliseconds>(next - now).count();
  const auto interval =
      static_cast<UINT>(std::clamp<std::int64_t>(delay, USER_TIMER_MINIMUM, USER_TIMER_MAXIMUM));
  if (!SetTimer(state.notification_window->host_hwnd, LIFECYCLE_TIMER_ID, interval, nullptr)) {
    Logger().error("Failed to schedule notification deadline: {}", GetLastError());
  } else {
    window.scheduled_deadline = next;
  }
}

// 依据卡片运动状态启闭悬停定时器：有卡片在运动则启动 16ms 检测，全部静止则停用
auto update_hover_timer(core::AppState& state) -> void {
  auto& window = *state.notification_window;
  const auto now = std::chrono::steady_clock::now();
  // 检查是否有任何未离场卡片处于位移运动中
  const bool moving = std::ranges::any_of(window.active_notifications, [&](const auto& card) {
    return card.lifetime.phase != NotificationPhase::Leaving && card.motion.deadline > now;
  });
  if (moving == window.hover_timer_active || !window.host_hwnd) {
    return;
  }
  if (moving) {
    // 启动 16ms 悬停检测定时器（仅在运动期间采样静止鼠标）
    window.hover_timer_active =
        SetTimer(window.host_hwnd, HOVER_TIMER_ID, HOVER_CHECK_INTERVAL_MS, nullptr) != 0;
    if (!window.hover_timer_active) {
      Logger().error("Failed to schedule notification hover checks: {}", GetLastError());
    }
  } else {
    // 所有卡片静止后立即停用定时器，进入零 CPU 占用
    KillTimer(window.host_hwnd, HOVER_TIMER_ID);
    window.hover_timer_active = false;
  }
}

// 触发单张卡片离场：启动 DComp 原地淡出动画 → 标记 Leaving 状态与截止时间 → 清除悬停高亮
auto begin_exit(Notification& notification, const animation::UpdateTime& time) -> void {
  if (notification.lifetime.phase == NotificationPhase::Leaving) {
    return;
  }
  // 启动 DComp 原地淡出动画
  animation::leave(notification.motion, time);
  notification.lifetime.phase = NotificationPhase::Leaving;
  notification.lifetime.deadline = time.lifetime + FADE_DURATION;
  notification.is_hovered = false;
  // 若之前有按钮高亮，标记内容脏以便清空高亮状态
  notification.content_dirty = notification.content_dirty || notification.action_hovered;
  notification.action_hovered = false;
}

auto active_layout_count(const core::AppState& state) -> size_t {
  size_t count = 0;
  for (const auto& notification : state.notification_window->active_notifications) {
    if (notification.lifetime.phase != NotificationPhase::Leaving) {
      ++count;
    }
  }
  return count;
}

// 更新通知宿主窗口边界与 DPI：检测 DPI 变化 → 重新计算窗口尺寸与位置 → 调用 SetWindowPos 同步
auto update_host_bounds(core::AppState& state) -> void {
  auto& window_state = *state.notification_window;
  if (!window_state.host_hwnd) {
    return;
  }

  // 获取当前显示器工作区并检测 DPI 变动
  const RECT work_area = get_notification_work_area(state);
  const int dpi = painter::get_current_dpi(state);
  window_state.layout_dirty = window_state.layout_dirty || window_state.dpi != dpi;
  window_state.dpi = dpi;
  const SIZE host_size = painter::get_host_size(window_state.dpi);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  // 尺寸或位置变化时更新 Win32 宿主窗口
  if (window_state.host_size.cx != host_size.cx || window_state.host_size.cy != host_size.cy ||
      window_state.host_position.x != host_position.x ||
      window_state.host_position.y != host_position.y) {
    window_state.layout_dirty = true;
    SetWindowPos(window_state.host_hwnd, HWND_TOPMOST, host_position.x, host_position.y,
                 host_size.cx, host_size.cy, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    window_state.host_size = host_size;
    window_state.host_position = host_position;
  }
}

// 测量所有卡片的局部排版与尺寸：DPI 变化时重测文本与高度 → 重置表面并标记脏
auto measure_notifications(core::AppState& state) -> void {
  const int dpi = painter::get_current_dpi(state);
  // DPI 改变时重测局部布局并替换表面，visual 保留以维持卡片的叠放顺序
  for (auto& notification : state.notification_window->active_notifications) {
    if (notification.dpi != dpi) {
      notification.width = painter::get_window_width(dpi);
      notification.layout = painter::compute_notification_layout(
          state, notification.message, notification.action, notification.width);
      notification.height = painter::measure_card_height(notification.layout, dpi);
      notification.dpi = dpi;
      notification.surface.reset();
      notification.content_dirty = true;
    }
  }
  // 更新各命中区域矩形相对原点的位置
  painter::update_all_notification_rects(state);
}

// 计算所有可见卡片的堆叠目标坐标：自底向上累加卡片高度与间距（跳过已离场卡片）
auto layout_notifications(core::AppState& state) -> void {
  const int dpi = state.notification_window->dpi;
  const int margin = painter::get_layout_margin(dpi);
  const int spacing = painter::scale_for_dpi(notification_window::BASE_SPACING, dpi);
  const int target_x = margin;
  int current_y = state.notification_window->host_size.cy - margin;

  // 自最新向最旧（自底向上）排版
  for (auto it = state.notification_window->active_notifications.rbegin();
       it != state.notification_window->active_notifications.rend(); ++it) {
    auto& notification = *it;
    // 离场中的卡片不占位
    if (notification.lifetime.phase == NotificationPhase::Leaving) {
      continue;
    }

    current_y -= notification.height;
    notification.layout_target = {static_cast<float>(target_x), static_cast<float>(current_y)};
    current_y -= spacing;
  }
}

// 更新所有卡片的运动目标：未入场卡片从屏外滑入，已有卡片向新目标平滑重定向
auto update_motion(core::AppState& state, const animation::UpdateTime& time) -> void {
  for (auto& notification : state.notification_window->active_notifications) {
    if (notification.lifetime.phase == NotificationPhase::Leaving) {
      continue;
    }
    // 首度入场：从屏幕右侧边缘滑入至目标位置
    if (notification.lifetime.phase == NotificationPhase::Entering &&
        notification.lifetime.deadline == std::chrono::steady_clock::time_point{}) {
      animation::enter(notification.motion,
                       {static_cast<float>(state.notification_window->host_size.cx),
                        notification.layout_target.y},
                       notification.layout_target, time);
      notification.lifetime.deadline = time.lifetime + SLIDE_DURATION;
    } else {
      // 已在场卡片：平滑位移到最新排版坐标
      animation::move_to(notification.motion, notification.layout_target, time);
    }
  }
}

// 统一向合成器发布卡片内容与运动：确保卡片资源 → 绘制脏表面 → 应用运动属性 → 提交事务
auto publish_notifications(core::AppState& state) -> void {
  auto& window = *state.notification_window;
  auto& resources = window.render_resources;
  // 检查是否有未提交的合成树拓扑、表面内容或运动属性
  const bool pending =
      window.composition_dirty ||
      std::ranges::any_of(window.active_notifications, [](const auto& card) {
        return card.content_dirty || card.motion.dirty || !card.surface || !card.visual;
      });
  if (!pending) {
    return;
  }
  if (!render_context::ensure_render_context(state) || resources.is_rendering) {
    return;
  }
  resources.is_rendering = true;
  bool success = true;
  // 遍历所有卡片：确保表面与 Visual → 脏内容重绘 → 应用运动
  for (auto& notification : state.notification_window->active_notifications) {
    if (!render_context::ensure_card(state, notification) ||
        (notification.content_dirty && !painter::paint_card(state, notification)) ||
        !render_context::apply_motion(state, notification)) {
      success = false;
      break;
    }
  }
  // 提交 DComp 事务并清空脏标记
  if (success && render_context::commit(state)) {
    window.composition_dirty = false;
    for (auto& notification : state.notification_window->active_notifications) {
      notification.content_dirty = false;
      notification.motion.dirty = false;
    }
  } else {
    render_context::cleanup_render_context(state);
  }
  resources.is_rendering = false;
}

auto show_host(core::AppState& state) -> void {
  if (!state.notification_window->host_hwnd) {
    return;
  }

  update_host_bounds(state);
  ShowWindow(state.notification_window->host_hwnd, SW_SHOWNA);
}

auto hide_host_if_idle(core::AppState& state) -> void {
  if (!state.notification_window->active_notifications.empty()) {
    return;
  }

  stop_lifecycle_timer(state);
  state.notification_window->hover_target = {};
  state.notification_window->pressed_target = {};
  if (state.notification_window->host_hwnd) {
    ShowWindow(state.notification_window->host_hwnd, SW_HIDE);
  }
}

auto find_notification(core::AppState& state, size_t id) -> std::list<Notification>::iterator {
  return std::ranges::find_if(
      state.notification_window->active_notifications,
      [id](const Notification& notification) { return notification.id == id; });
}

// 在指定时刻对卡片执行命中测试：自上而下遍历 → CPU 采样瞬时坐标 → 判定动作按钮、正文与卡片区域
auto hit_test_at(core::AppState& state, POINT point, LARGE_INTEGER time) -> NotificationHitTarget {
  if (state.notification_window->active_notifications.empty()) {
    return {};
  }

  auto& notifications = state.notification_window->active_notifications;
  // 自顶向下（最新卡片在最前）进行命中测试
  for (auto it = notifications.rbegin(); it != notifications.rend(); ++it) {
    const auto& notification = *it;
    // 离场中或几乎完全透明的卡片不响应点击
    if (notification.lifetime.phase == NotificationPhase::Leaving ||
        ui::composition_animation::sample(notification.motion.opacity, time) <= 0.05f) {
      continue;
    }

    // 采样卡片当前物理瞬时坐标并转换为卡片局部坐标
    const auto position = animation::sample_position(notification.motion, time);
    const float local_x = point.x - position.x;
    const float local_y = point.y - position.y;
    const auto contains = [local_x, local_y](const RECT& rect) {
      return local_x >= rect.left && local_x < rect.right && local_y >= rect.top &&
             local_y < rect.bottom;
    };
    // 优先命中操作按钮
    if (notification.action && contains(notification.action_rect)) {
      return {.kind = NotificationHitKind::Action, .notification_id = notification.id};
    }
    // 其次命中正文内容区域
    if (contains(notification.content_rect)) {
      return {.kind = NotificationHitKind::Content, .notification_id = notification.id};
    }
    // 命中卡片主体背景
    if (contains(notification.card_rect)) {
      return {.kind = NotificationHitKind::Card, .notification_id = notification.id};
    }
  }

  return {};
}

// 更新鼠标悬停状态并控制生命周期倒计时：追踪鼠标离开 → 悬停时暂停倒计时，移出后恢复 → 标记按钮高亮
auto update_hover_at(core::AppState& state, NotificationHitTarget target,
                     std::chrono::steady_clock::time_point now) -> void {
  auto& window = *state.notification_window;
  // 合成动画可在没有 WM_MOUSEMOVE 的情况下进入鼠标下方，也要跟踪后续离开
  if (!hit_targets_equal(window.hover_target, target) && target.kind != NotificationHitKind::None) {
    TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window.host_hwnd, 0};
    TrackMouseEvent(&tracking);
  }
  window.hover_target = target;

  for (auto& notification : state.notification_window->active_notifications) {
    const bool hovered =
        target.kind != NotificationHitKind::None && target.notification_id == notification.id;
    // 卡片悬停状态改变：暂停或恢复展示生命周期倒计时
    if (notification.is_hovered != hovered) {
      notification.is_hovered = hovered;

      if (notification.lifetime.phase == NotificationPhase::Visible) {
        if (hovered) {
          // 悬停时暂停计时，记录剩余有效展示时长
          notification.lifetime.remaining_display_time = std::max(
              notification.lifetime.deadline - now, std::chrono::steady_clock::duration::zero());
        } else {
          // 移开时恢复计时，重新计算截止时间点
          notification.lifetime.deadline = now + notification.lifetime.remaining_display_time;
        }
      }
    }

    // 更新操作按钮悬停高亮
    const bool action_hovered =
        target.kind == NotificationHitKind::Action && target.notification_id == notification.id;
    if (notification.action_hovered != action_hovered) {
      notification.action_hovered = action_hovered;
      notification.content_dirty = true;
    }
  }
}

auto hit_test_notifications(core::AppState& state, POINT point) -> NotificationHitTarget {
  return hit_test_at(state, point, animation::now().motion);
}

auto update_hover_state(core::AppState& state, NotificationHitTarget target) -> bool {
  if (hit_targets_equal(state.notification_window->hover_target, target)) {
    return false;
  }
  update_hover_at(state, target, animation::now().lifetime);
  publish_notifications(state);
  schedule_lifecycle_timer(state);
  return true;
}

// 获取当前鼠标屏幕位置并刷新各卡片的悬停状态
auto refresh_hover(core::AppState& state, const animation::UpdateTime& time) -> void {
  POINT cursor{};
  if (GetCursorPos(&cursor) && ScreenToClient(state.notification_window->host_hwnd, &cursor)) {
    update_hover_at(state, hit_test_at(state, cursor, time.motion), time.lifetime);
  }
}

// 运动期间定时器回调：采样鼠标位置 → 刷新卡片悬停 → 发布高亮变更 → 评估定时器状态
auto check_hover(core::AppState& state) -> void {
  if (!state.notification_window->hover_timer_active) {
    return;
  }
  refresh_hover(state, animation::now());
  publish_notifications(state);
  schedule_lifecycle_timer(state);
  update_hover_timer(state);
}

auto execute_action_callback(core::AppState& state, const NotificationHitTarget& target) -> void {
  if (target.kind != NotificationHitKind::Action) {
    return;
  }

  auto it = find_notification(state, target.notification_id);
  if (it == state.notification_window->active_notifications.end() || !it->action ||
      !it->action->callback) {
    return;
  }

  auto callback = it->action->callback;
  try {
    callback(state);
  } catch (const std::exception& e) {
    Logger().error("Notification action callback failed: {}", e.what());
  } catch (...) {
    Logger().error("Notification action callback failed with an unknown exception");
  }
}

// 处理鼠标左键抬起点击：校验按下与释放目标一致性 → 触发按钮回调 → 触发卡片退出并重排
auto handle_click_release(core::AppState& state, NotificationHitTarget release_target) -> void {
  const auto pressed_target = state.notification_window->pressed_target;
  state.notification_window->pressed_target = {};
  if (!hit_targets_equal(pressed_target, release_target)) {
    request_repaint(state);
    return;
  }

  // 执行动作按钮绑定的回调操作
  if (release_target.kind == NotificationHitKind::Action) {
    execute_action_callback(state, release_target);
  }

  // 点击卡片正文或操作按钮后，该通知进入离场淡出流程
  if (release_target.kind == NotificationHitKind::Content ||
      release_target.kind == NotificationHitKind::Action) {
    auto it = find_notification(state, release_target.notification_id);
    if (it != state.notification_window->active_notifications.end()) {
      const auto time = animation::now();
      begin_exit(*it, time);
      state.notification_window->layout_dirty = true;
      update_at(state, time);
      return;
    }
  }

  request_repaint(state);
}

// 执行通知系统的单次状态推进：度量排版 → 检查生命周期与阶段跃迁 → 重新计算目标位移 →
// 统一发布并调度定时器
auto update_at(core::AppState& state, const animation::UpdateTime& time) -> void {
  auto& window = *state.notification_window;
  if (!window.host_hwnd) {
    return;
  }
  // 若布局脏则重新度量卡片尺寸
  if (window.layout_dirty) {
    measure_notifications(state);
  }
  // 刷新鼠标悬停状态
  refresh_hover(state, time);
  // 遍历活跃卡片，推进生命周期状态机
  for (auto it = window.active_notifications.begin(); it != window.active_notifications.end();) {
    auto& notification = *it;
    auto& lifetime = notification.lifetime;
    // 检查卡片位移/渐变动画是否已到达终点
    if (notification.motion.deadline <= time.lifetime) {
      notification.motion.deadline = {};
    }
    // 入场滑入结束，转入静止展示阶段并启动展示倒计时
    if (lifetime.phase == NotificationPhase::Entering &&
        lifetime.deadline != std::chrono::steady_clock::time_point{} &&
        time.lifetime >= lifetime.deadline) {
      lifetime.phase = NotificationPhase::Visible;
      lifetime.remaining_display_time = notification.duration;
      lifetime.deadline += notification.duration;
    }
    // 展示倒计时到期且未被悬停暂停，触发离场淡出
    if (lifetime.phase == NotificationPhase::Visible && !notification.is_hovered &&
        time.lifetime >= lifetime.deadline) {
      begin_exit(notification, time);
      window.layout_dirty = true;
    }
    // 离场淡出动画结束，释放合成资源并从活跃列表中移除
    if (lifetime.phase == NotificationPhase::Leaving && time.lifetime >= lifetime.deadline) {
      render_context::remove_card(state, notification);
      it = window.active_notifications.erase(it);
    } else {
      ++it;
    }
  }
  // 若卡片增减或退出导致排版变动，重新计算目标坐标并更新位移动画
  if (window.layout_dirty) {
    layout_notifications(state);
    update_motion(state, time);
    window.layout_dirty = false;
  }
  // 退出会暴露下面的卡片，重排也可能改变静止鼠标的命中对象
  refresh_hover(state, time);
  // 向系统合成器发布所有卡片的内容与运动
  publish_notifications(state);
  // 评估并调度下一个生命周期定时器与运动悬停定时器
  schedule_lifecycle_timer(state);
  update_hover_timer(state);
  // 全部卡片离场后隐藏宿主窗口
  hide_host_if_idle(state);
}

auto update_notifications(core::AppState& state) -> void { update_at(state, animation::now()); }

// 处理生命周期定时器唤醒：停止当前定时器 → 推进通知状态机
auto handle_lifecycle_timer(core::AppState& state) -> void {
  stop_lifecycle_timer(state);
  update_notifications(state);
}

auto initialize(core::AppState& state) -> std::expected<void, std::string> {
  if (!state.notification_window) {
    return std::unexpected("Notification window state is not allocated");
  }

  HINSTANCE instance = state.floating_window->window.instance;
  if (!instance) {
    instance = GetModuleHandleW(nullptr);
  }

  if (!register_host_window_class(instance)) {
    return std::unexpected("Failed to register notification host window class");
  }

  return {};
}

auto cleanup(core::AppState& state) -> void {
  stop_lifecycle_timer(state);
  if (state.notification_window->host_hwnd && state.notification_window->hover_timer_active) {
    KillTimer(state.notification_window->host_hwnd, HOVER_TIMER_ID);
  }
  state.notification_window->hover_timer_active = false;

  if (state.notification_window->host_hwnd) {
    DestroyWindow(state.notification_window->host_hwnd);
    state.notification_window->host_hwnd = nullptr;
  } else {
    ui::notification_window::render_context::cleanup_render_context(state);
  }

  state.notification_window->active_notifications.clear();
  state.notification_window->hover_target = {};
  state.notification_window->pressed_target = {};
  state.notification_window->host_size = {};
  state.notification_window->host_position = {};
  state.notification_window->dpi = 96;
  state.notification_window->next_id = 0;
  state.notification_window->layout_dirty = true;
  state.notification_window->composition_dirty = false;
}

// 显示一条新通知：确保宿主窗口与渲染上下文 → 计算文本排版与测高 → 超限则让最旧卡片退场 →
// 推进状态并提交
auto show_notification(core::AppState& state, core::notifications::NotificationOptions options)
    -> void {
  // 确保宿主窗口已创建
  if (!ensure_host_window(state)) {
    return;
  }
  // 确保 D2D 与 DComp 渲染上下文就绪
  if (!ui::notification_window::render_context::ensure_render_context(state)) {
    Logger().error("Failed to initialize notification render context");
    return;
  }

  // 计算当前 DPI 下的卡片宽度、文本排版指标与高度
  const int dpi = painter::get_current_dpi(state);
  const int card_width = painter::get_window_width(dpi);
  const auto action = painter::normalize_action(std::move(options.action));
  const auto layout =
      painter::compute_notification_layout(state, options.message, action, card_width);

  const auto time = animation::now();
  // 超过最大并发可见数（3张）时，将最旧的活跃卡片提前进入离场淡出
  if (active_layout_count(state) >=
      static_cast<size_t>(notification_window::MAX_VISIBLE_NOTIFICATIONS)) {
    for (auto& notification : state.notification_window->active_notifications) {
      if (notification.lifetime.phase != NotificationPhase::Leaving) {
        begin_exit(notification, time);
        break;
      }
    }
  }

  // 构造新通知对象
  Notification notification{
      .id = state.notification_window->next_id++,
      .title = std::move(options.title),
      .message = std::move(options.message),
      .action = action,
      .colors = painter::resolve_notification_theme_colors(state),
      .duration = options.duration,
  };
  notification.dpi = dpi;
  notification.width = card_width;
  notification.layout = layout;
  notification.height = painter::measure_card_height(layout, dpi);

  // 加入活跃列表，标记排版脏并推进状态提交到合成器
  state.notification_window->active_notifications.emplace_back(std::move(notification));
  state.notification_window->layout_dirty = true;
  update_host_bounds(state);
  update_at(state, time);
  show_host(state);
}

auto request_repaint(core::AppState& state) -> void { painter::request_repaint(state); }

}  // namespace ui::notification_window
