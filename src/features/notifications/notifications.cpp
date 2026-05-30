module;

module Features.Notifications;

import std;
import Core.Events;
import Core.State;
import Features.Notifications.Events;
import Features.Notifications.State;
import Features.Notifications.Constants;
import Features.Notifications.Types;
import Features.Notifications.Draw;
import UI.FloatingWindow.State;
import Utils.Display;
import Utils.Logger;
import Utils.String;
import <windows.h>;
import <windowsx.h>;

namespace Features::Notifications {

// ============================================================
// 通知模块总体架构
// ============================================================
//
// 整个通知系统由一个"透明宿主窗口"驱动，而不是每条通知各建一个窗口。
// 所有 toast 卡片都是用 Direct2D (D2D) 在同一张离屏位图上绘制的图形，
// 绘制完成后通过 UpdateLayeredWindow 一次性提交给 DWM，实现逐像素透明。
//
// 数据流：
//   show_notification(title, message)     ← UI 线程、无 action
//   post_notification_request(options)    ← 非 UI 线程或有 action/duration
//       ↓ Events::post → UI 线程 show_notification(options)
//   show_notification(options)            ← 创建/唤醒宿主窗口
//       ↓ 测量布局、加入 active_notifications 列表
//       ↓ 启动 WM_TIMER（~16ms / 帧）
//
//   WM_TIMER → update_notifications()
//       ↓ 推进每条通知的动画状态（SlidingIn / Displaying / MovingUp / FadingOut）
//       ↓ 删除 Done 状态的通知
//       ↓ paint_notifications() → 重绘所有卡片 → present_render_surface()
//
// 渲染资源（NotificationRenderSurface）在宿主窗口首次绘制时按需创建，
// 宿主窗口隐藏后保留资源，下次通知到来可以直接复用，避免反复初始化 D2D。
//
// 渲染实现见 Features.Notifications.Draw 子模块。

// ============================================================
// 动画数学辅助
// ============================================================

// 三次缓出（ease-out cubic）：动画开始快、结尾慢，接近自然物理感。
// t=0 时输出 0，t=1 时输出 1，中间段速度先快后慢。
auto ease_out_cubic(float t) -> float {
  const float ft = 1.0f - t;
  return 1.0f - ft * ft * ft;
}

// 把"已过去的时长"映射到 [0, 1] 的动画进度，超过 SLIDE_DURATION 则钳制为 1。
auto slide_progress(std::chrono::steady_clock::duration elapsed) -> float {
  return std::min(
      1.0f,
      static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
          Constants::SLIDE_DURATION.count());
}

// 在 start 和 target 之间按 eased 值做线性插值，用于平滑移动 toast 的屏幕坐标。
auto lerp_pos(POINT start, POINT target, float eased) -> POINT {
  return {start.x + static_cast<int>((target.x - start.x) * eased),
          start.y + static_cast<int>((target.y - start.y) * eased)};
}

// ============================================================
// 通用谓词 / 几何辅助
// ============================================================

// 比较两个命中目标是否指向同一个通知的同一个区域，用于验证鼠标按下/抬起是否配对。
auto hit_targets_equal(const State::NotificationHitTarget& left,
                       const State::NotificationHitTarget& right) -> bool {
  return left.kind == right.kind && left.notification_id == right.notification_id;
}

// 点是否在矩形内（右边界和下边界不包含在内，与 Win32 惯例一致）。
auto rect_contains(const RECT& rect, const POINT& point) -> bool {
  return point.x >= rect.left && point.x < rect.right && point.y >= rect.top &&
         point.y < rect.bottom;
}
// ============================================================
// 宿主窗口管理
// ============================================================

auto register_host_window_class(HINSTANCE instance) -> bool;
LRESULT CALLBACK NotificationHostWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 锚定浮窗所在显示器的工作区；浮窗不可见时回退主屏（与 overlay/preview 一致）。
auto get_notification_work_area(const Core::State::AppState& state) -> RECT {
  const auto& fw = *state.floating_window;
  auto monitor = Utils::Display::get_working_monitor(fw.window.hwnd, fw.window.is_visible);
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

auto update_host_bounds(Core::State::AppState& state) -> void {
  // 宿主窗口锚定在浮窗所在屏的工作区右下角；任务栏位置或 DPI 改变后需要重新计算。
  if (!state.notifications->host_hwnd) {
    return;
  }

  const RECT work_area = get_notification_work_area(state);

  state.notifications->dpi = get_current_dpi(state);
  const SIZE host_size = get_host_size(state.notifications->dpi);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  if (state.notifications->host_size.cx != host_size.cx ||
      state.notifications->host_size.cy != host_size.cy ||
      state.notifications->host_position.x != host_position.x ||
      state.notifications->host_position.y != host_position.y) {
    SetWindowPos(state.notifications->host_hwnd, HWND_TOPMOST, host_position.x, host_position.y,
                 host_size.cx, host_size.cy, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    state.notifications->host_size = host_size;
    state.notifications->host_position = host_position;
  }
}

// 确保透明宿主窗口已创建。
// WS_EX_LAYERED 使窗口支持逐像素透明（通过 UpdateLayeredWindow）。
// WS_EX_TOOLWINDOW 让窗口不在任务栏出现。
// WS_EX_TOPMOST 确保通知始终显示在最前。
// CreateWindowExW 的最后一个参数 &state 会在 WM_NCCREATE 中取出，存入 GWLP_USERDATA。
auto ensure_host_window(Core::State::AppState& state) -> bool {
  // 通知系统只有一个透明宿主窗口。toast 本身不是子窗口，只是画在这张画布上的卡片。
  if (state.notifications->host_hwnd) {
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

  state.notifications->dpi = get_current_dpi(state);
  const SIZE host_size = get_host_size(state.notifications->dpi);
  const RECT work_area = get_notification_work_area(state);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  HWND hwnd =
      CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                      Constants::NOTIFICATION_WINDOW_CLASS.c_str(), L"SpinningMomoNotifications",
                      WS_POPUP | WS_CLIPCHILDREN, host_position.x, host_position.y, host_size.cx,
                      host_size.cy, nullptr, nullptr, instance, &state);
  if (!hwnd) {
    Logger().error("Failed to create notification host window. Error: {}", GetLastError());
    return false;
  }

  state.notifications->host_hwnd = hwnd;
  state.notifications->host_size = host_size;
  state.notifications->host_position = host_position;
  return true;
}
auto start_animation_timer(Core::State::AppState& state) -> void {
  // 只有存在动画或可见通知时才启动 timer；空闲时关闭，避免 UI 线程无意义唤醒。
  if (!state.notifications->animation_timer_active && state.notifications->host_hwnd) {
    SetTimer(state.notifications->host_hwnd, Constants::ANIMATION_TIMER_ID,
             Constants::ANIMATION_FRAME_INTERVAL, nullptr);
    state.notifications->animation_timer_active = true;
  }
}

// 停止 timer。animation_timer_active 置 false 无论 KillTimer 是否执行，
// 确保状态一致（例如窗口销毁时 hwnd 已无效，仍需重置标志）。
auto stop_animation_timer(Core::State::AppState& state) -> void {
  if (state.notifications->animation_timer_active && state.notifications->host_hwnd) {
    KillTimer(state.notifications->host_hwnd, Constants::ANIMATION_TIMER_ID);
  }
  state.notifications->animation_timer_active = false;
}

auto mark_notification_fading_out(State::Notification& notification,
                                  std::chrono::steady_clock::time_point now) -> void {
  // 进入淡出时保存当前坐标和透明度，确保从用户看到的那一帧平滑退出。
  if (notification.state == Types::NotificationAnimState::FadingOut ||
      notification.state == Types::NotificationAnimState::Done) {
    return;
  }

  notification.state = Types::NotificationAnimState::FadingOut;
  notification.last_state_change_time = now;
  notification.animation_start_pos = notification.current_pos;
  notification.animation_start_opacity = notification.opacity;
  notification.is_hovered = false;
  notification.action_hovered = false;
}

// ============================================================
// 动画状态机
// ============================================================
//
// 每条通知的生命周期：
//   Spawning → SlidingIn → Displaying → FadingOut → Done（被删除）
//                                  ↑
//                             MovingUp（为新通知让位时插入此状态）
//
// 状态转换由 update_notification_animation() 按真实时间驱动，
// 不依赖帧数，因此 timer 抖动不会影响动画时长。

// 统计当前不处于退出阶段的通知数量，用于判断是否需要挤出最旧的通知腾出位置。
auto active_layout_count(const Core::State::AppState& state) -> size_t {
  size_t count = 0;
  for (const auto& notification : state.notifications->active_notifications) {
    if (!is_exiting(notification.state)) {
      ++count;
    }
  }
  return count;
}

auto relayout_notifications(Core::State::AppState& state, std::chrono::steady_clock::time_point now)
    -> void {
  // 布局从底部往上排。新通知加入或旧通知移除时，只改变目标位置；
  // 真正的位置变化由动画阶段逐帧插值完成。
  const int dpi = get_current_dpi(state);
  const int margin = get_layout_margin(dpi);
  const int spacing = scale_for_dpi(Constants::BASE_SPACING, dpi);
  const int target_x = margin;
  int current_y = state.notifications->host_size.cy - margin;

  for (auto it = state.notifications->active_notifications.rbegin();
       it != state.notifications->active_notifications.rend(); ++it) {
    auto& notification = *it;
    if (is_exiting(notification.state)) {
      continue;
    }

    current_y -= notification.height;
    const POINT new_target{target_x, current_y};

    if (notification.state == Types::NotificationAnimState::Spawning) {
      // 新通知从宿主窗口右侧滑入，目标位置已经是右下栈中的最终位置。
      notification.target_pos = new_target;
      notification.current_pos = {state.notifications->host_size.cx, new_target.y};
      notification.animation_start_pos = notification.current_pos;
      notification.animation_start_opacity = 0.0f;
      notification.opacity = 0.0f;
      notification.state = Types::NotificationAnimState::SlidingIn;
      notification.last_state_change_time = now;
    } else if (notification.target_pos.y != new_target.y ||
               notification.target_pos.x != new_target.x) {
      notification.target_pos = new_target;
      if (notification.state == Types::NotificationAnimState::Displaying ||
          notification.state == Types::NotificationAnimState::MovingUp) {
        // 已显示的通知只做垂直平移动画，为新通知或被删除通知让位。
        notification.animation_start_pos = notification.current_pos;
        notification.animation_start_opacity = notification.opacity;
        notification.state = Types::NotificationAnimState::MovingUp;
        notification.last_state_change_time = now;
      }
    }

    current_y -= spacing;
  }

  update_all_notification_rects(state);
}

// ============================================================
// 鼠标交互 / 窗口生命周期
// ============================================================

// 显示宿主窗口并刷新位置。SW_SHOWNA 不激活、不抢焦点，游戏窗口的输入不受干扰。
auto show_host(Core::State::AppState& state) -> void {
  if (!state.notifications->host_hwnd) {
    return;
  }

  update_host_bounds(state);
  ShowWindow(state.notifications->host_hwnd, SW_SHOWNA);
}

auto hide_host_if_idle(Core::State::AppState& state) -> void {
  // 没有通知时隐藏宿主窗口而不是销毁，下一次通知可以复用 D2D 资源。
  if (!state.notifications->active_notifications.empty()) {
    return;
  }

  stop_animation_timer(state);
  state.notifications->hover_target = {};
  state.notifications->pressed_target = {};
  if (state.notifications->host_hwnd) {
    ShowWindow(state.notifications->host_hwnd, SW_HIDE);
  }
}

// 按 id 在活跃通知列表中查找，返回迭代器（找不到时返回 end()）。
// 列表规模很小（最多 MAX_VISIBLE_NOTIFICATIONS），O(n) 顺序查找足够。
auto find_notification(Core::State::AppState& state, size_t id)
    -> std::list<State::Notification>::iterator {
  return std::ranges::find_if(
      state.notifications->active_notifications,
      [id](const State::Notification& notification) { return notification.id == id; });
}

auto hit_test_notifications(Core::State::AppState& state, POINT point)
    -> State::NotificationHitTarget {
  // 反向遍历表示优先命中视觉上更靠上的 toast。
  // 返回 None 时，WM_NCHITTEST 会让宿主透明区域穿透到后面的窗口。
  if (state.notifications->active_notifications.empty()) {
    return {};
  }

  auto& notifications = state.notifications->active_notifications;
  for (auto it = notifications.rbegin(); it != notifications.rend(); ++it) {
    const auto& notification = *it;
    if (is_exiting(notification.state) || notification.opacity <= 0.05f) {
      continue;
    }

    if (notification.action && rect_contains(notification.action_rect, point)) {
      return {.kind = State::NotificationHitKind::Action, .notification_id = notification.id};
    }

    if (rect_contains(notification.content_rect, point)) {
      return {.kind = State::NotificationHitKind::Content, .notification_id = notification.id};
    }

    if (rect_contains(notification.card_rect, point)) {
      return {.kind = State::NotificationHitKind::Card, .notification_id = notification.id};
    }
  }

  return {};
}

auto update_hover_state(Core::State::AppState& state, State::NotificationHitTarget target) -> bool {
  // 悬停不仅影响按钮高亮，也会暂停通知的自动关闭计时。
  // 离开时把暂停时长累计进去，后续显示时长仍按“用户实际看到的时间”计算。
  const bool target_changed = !hit_targets_equal(state.notifications->hover_target, target);
  state.notifications->hover_target = target;

  bool changed = target_changed;
  const auto now = std::chrono::steady_clock::now();
  for (auto& notification : state.notifications->active_notifications) {
    const bool hovered = target.kind != State::NotificationHitKind::None &&
                         target.notification_id == notification.id;
    if (notification.is_hovered != hovered) {
      notification.is_hovered = hovered;
      changed = true;

      if (hovered && notification.state == Types::NotificationAnimState::Displaying) {
        notification.pause_start_time = now;
      } else if (!hovered && notification.state == Types::NotificationAnimState::Displaying &&
                 notification.pause_start_time.time_since_epoch().count() > 0) {
        const auto paused_duration = now - notification.pause_start_time;
        notification.total_paused_duration +=
            std::chrono::duration_cast<std::chrono::milliseconds>(paused_duration);
        notification.pause_start_time = {};
      }
    }

    const bool action_hovered = target.kind == State::NotificationHitKind::Action &&
                                target.notification_id == notification.id;
    if (notification.action_hovered != action_hovered) {
      notification.action_hovered = action_hovered;
      changed = true;
    }
  }

  return changed;
}

// 推进单条通知的动画状态，返回 true 表示本帧有视觉变化（需要重绘）。
// Displaying 状态下鼠标悬停时不推进时间（is_hovered），模拟"暂停"效果。
// 累计暂停时长从 elapsed_time 中扣除，确保用户实际看到的时间满足 duration。
auto update_notification_animation(State::Notification& notification,
                                   std::chrono::steady_clock::time_point now) -> bool {
  // 动画按真实时间推进，不按帧数推进；timer 抖动时动画时长仍然稳定。
  auto elapsed_time = now - notification.last_state_change_time;
  if (notification.state == Types::NotificationAnimState::Displaying && notification.is_hovered) {
    return false;
  }
  if (notification.state == Types::NotificationAnimState::Displaying) {
    elapsed_time -= notification.total_paused_duration;
  }

  switch (notification.state) {
    case Types::NotificationAnimState::SlidingIn: {
      const float p = slide_progress(elapsed_time);
      const float eased = ease_out_cubic(p);
      notification.current_pos =
          lerp_pos(notification.animation_start_pos, notification.target_pos, eased);
      notification.opacity = eased;
      if (p >= 1.0f) {
        notification.state = Types::NotificationAnimState::Displaying;
        notification.last_state_change_time = now;
        notification.display_started_time = now;
        notification.current_pos = notification.target_pos;
        notification.opacity = 1.0f;
        notification.total_paused_duration = std::chrono::milliseconds(0);
        notification.pause_start_time = {};
      }
      return true;
    }

    case Types::NotificationAnimState::Displaying:
      if (!notification.is_hovered && elapsed_time >= notification.duration) {
        mark_notification_fading_out(notification, now);
        return true;
      }
      return false;

    case Types::NotificationAnimState::MovingUp: {
      const float p = slide_progress(elapsed_time);
      notification.current_pos =
          lerp_pos(notification.animation_start_pos, notification.target_pos, ease_out_cubic(p));
      if (p >= 1.0f) {
        notification.state = Types::NotificationAnimState::Displaying;
        notification.last_state_change_time = now;
        notification.current_pos = notification.target_pos;
      }
      return true;
    }

    // FadingOut 使用独立的 FADE_DURATION（可与 SLIDE_DURATION 不同），
    // opacity 从进入淡出时的初始值线性降到 0，而不是从 1 开始，
    // 这样中途被打断的通知不会出现透明度跳变。
    case Types::NotificationAnimState::FadingOut: {
      const float progress = std::min(
          1.0f, static_cast<float>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()) /
                    Constants::FADE_DURATION.count());
      notification.opacity =
          notification.animation_start_opacity * (1.0f - ease_out_cubic(progress));
      if (progress >= 1.0f) {
        notification.state = Types::NotificationAnimState::Done;
      }
      return true;
    }

    case Types::NotificationAnimState::Spawning:
    case Types::NotificationAnimState::Done:
      return false;
  }

  return false;
}

auto execute_action_callback(Core::State::AppState& state,
                             const State::NotificationHitTarget& target) -> void {
  if (target.kind != State::NotificationHitKind::Action) {
    return;
  }

  auto it = find_notification(state, target.notification_id);
  if (it == state.notifications->active_notifications.end() || !it->action ||
      !it->action->callback) {
    return;
  }

  // 先复制 callback，再执行。callback 内部可能显示新通知或修改通知列表，
  // 后续关闭当前通知时会重新按 id 查找，避免持有失效迭代器。
  auto callback = it->action->callback;
  try {
    callback(state);
  } catch (const std::exception& e) {
    Logger().error("Notification action callback failed: {}", e.what());
  } catch (...) {
    Logger().error("Notification action callback failed with an unknown exception");
  }
}

auto handle_click_release(Core::State::AppState& state, State::NotificationHitTarget release_target)
    -> void {
  // 鼠标按下和抬起必须命中同一个目标，才算一次有效点击。
  // 这样拖出按钮再松开不会误触发 action。
  const auto pressed_target = state.notifications->pressed_target;
  state.notifications->pressed_target = {};
  if (!hit_targets_equal(pressed_target, release_target)) {
    request_repaint(state);
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (release_target.kind == State::NotificationHitKind::Action) {
    execute_action_callback(state, release_target);
  }

  if (release_target.kind == State::NotificationHitKind::Content ||
      release_target.kind == State::NotificationHitKind::Action) {
    auto it = find_notification(state, release_target.notification_id);
    if (it != state.notifications->active_notifications.end()) {
      mark_notification_fading_out(*it, now);
      relayout_notifications(state, now);
    }
  }

  request_repaint(state);
}

// ============================================================
// 公共接口
// ============================================================

// 显示一条通知的完整入口。负责把调用方传入的 options 固化为运行时 Notification 结构：
//   1. 若活跃通知已满，挤出队列中最早的一条（让它立即开始淡出）
//   2. 测量布局尺寸（需要渲染表面已就绪）
//   3. 将通知加入列表，进入 Spawning 状态
//   4. 触发 relayout、显示窗口、启动 timer、立即绘制第一帧
auto show_notification(Core::State::AppState& state, Types::NotificationOptions options) -> void {
  // 公开入口负责把业务层传入的 options 固化成运行时 Notification：
  // 创建宿主窗口、测量高度、记录颜色快照、加入列表并启动动画。
  if (!ensure_host_window(state)) {
    return;
  }
  if (!ensure_render_surface(state)) {
    Logger().error("Failed to initialize notification render surface");
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (active_layout_count(state) >= static_cast<size_t>(Constants::MAX_VISIBLE_NOTIFICATIONS)) {
    for (auto& notification : state.notifications->active_notifications) {
      if (!is_exiting(notification.state)) {
        mark_notification_fading_out(notification, now);
        break;
      }
    }
  }

  const int dpi = get_current_dpi(state);
  const int card_width = get_window_width(dpi);
  const auto action = normalize_action(std::move(options.action));
  const auto layout = compute_notification_layout(state, options.message, action, card_width);

  State::Notification notification{
      .id = state.notifications->next_id++,
      .title = std::move(options.title),
      .message = std::move(options.message),
      .action = action,
      .colors = resolve_notification_theme_colors(state),
      .state = Types::NotificationAnimState::Spawning,
      .last_state_change_time = now,
      .duration = options.duration,
  };
  notification.width = card_width;
  notification.layout = layout;
  notification.height = measure_card_height(layout, dpi);

  state.notifications->active_notifications.emplace_back(std::move(notification));
  update_host_bounds(state);
  relayout_notifications(state, now);
  show_host(state);
  start_animation_timer(state);
  paint_notifications(state);
}

auto post_notification_request(Core::State::AppState& state, Types::NotificationOptions options)
    -> void {
  Core::Events::post(state, Events::NotificationRequestEvent{.options = std::move(options)});
}

// UI 线程、无 action：直接显示，不经过事件队列。
auto show_notification(Core::State::AppState& state, const std::string& title,
                       const std::string& message) -> void {
  Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(title);
  options.message = Utils::String::FromUtf8(message);
  show_notification(state, std::move(options));
}

auto update_notifications(Core::State::AppState& state) -> void {
  // 这是宿主窗口 WM_TIMER 的驱动函数。它只推进状态和清理已完成通知；
  // 绘制仍统一走 paint_notifications()。
  if (!state.notifications->host_hwnd) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  bool needs_repaint = false;
  bool erased = false;

  for (auto it = state.notifications->active_notifications.begin();
       it != state.notifications->active_notifications.end();) {
    auto& notification = *it;
    needs_repaint = update_notification_animation(notification, now) || needs_repaint;

    if (notification.state == Types::NotificationAnimState::Done) {
      it = state.notifications->active_notifications.erase(it);
      erased = true;
    } else {
      ++it;
    }
  }

  if (erased) {
    relayout_notifications(state, now);  // 内部已调 update_all_notification_rects
    needs_repaint = true;
  } else if (needs_repaint) {
    update_all_notification_rects(state);  // 动画帧位置更新后同步 rects
  }

  if (state.notifications->active_notifications.empty()) {
    hide_host_if_idle(state);
    return;
  }

  if (needs_repaint) {
    paint_notifications(state);
  }
}

// ============================================================
// 窗口类注册 & 消息处理
// ============================================================

// 注册宿主窗口类，用 static bool 防止重复注册。
// 若 RegisterClassExW 因"已存在"而失败（另一个模块/进程已注册），也视为成功。
auto register_host_window_class(HINSTANCE instance) -> bool {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = NotificationHostWindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = Constants::NOTIFICATION_WINDOW_CLASS.c_str();
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  if (!RegisterClassExW(&wc)) {
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  registered = true;
  return true;
}

// 宿主窗口的消息处理函数。Win32 的惯用写法：
//   WM_NCCREATE  — 窗口刚创建，从 CREATESTRUCT 取出 &state 指针存入 GWLP_USERDATA。
//                  这样后续所有消息都能通过 GetWindowLongPtrW 取回 state，
//                  避免使用全局变量。
//   WM_NCHITTEST — 命中测试：非卡片区域返回 HTTRANSPARENT，
//                  鼠标事件会穿透到下方的游戏窗口。
//   WM_MOUSEMOVE — 更新 hover 状态；同时调用 TrackMouseEvent 以订阅 WM_MOUSELEAVE。
//   WM_MOUSELEAVE — 鼠标离开窗口时清除所有 hover。
//   WM_LBUTTONDOWN/UP — 实现按下/抬起配对点击，防止拖出后误触 action。
//   WM_TIMER    — 动画帧驱动，固定 ~16ms 调用 update_notifications。
//   WM_DPICHANGED / WM_DISPLAYCHANGE / WM_SETTINGCHANGE — 屏幕分辨率或任务栏位置变化时重新定位。
//   WM_NCDESTROY — 窗口销毁时清理 D2D 资源，重置 hwnd 指针。
LRESULT CALLBACK NotificationHostWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // 宿主窗口过程把 Win32 消息翻译成通知系统的高层动作：
  // 命中测试、hover、点击、timer、DPI/工作区变化。
  Core::State::AppState* state = nullptr;
  if (msg == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    if (state) {
      state->notifications->host_hwnd = hwnd;
    }
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (!state) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      BeginPaint(hwnd, &ps);
      paint_notifications(*state);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hwnd, &point);
      const auto target = hit_test_notifications(*state, point);
      // 非卡片区域返回 HTTRANSPARENT，让鼠标事件落到后面的游戏或桌面窗口。
      return target.kind == State::NotificationHitKind::None ? HTTRANSPARENT : HTCLIENT;
    }

    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      const auto target = hit_test_notifications(*state, point);
      if (update_hover_state(*state, target)) {
        request_repaint(*state);
      }

      TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tme);
      return 0;
    }

    case WM_MOUSELEAVE: {
      if (update_hover_state(*state, {})) {
        request_repaint(*state);
      }
      return 0;
    }

    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      const auto target = hit_test_notifications(*state, point);
      if (target.kind == State::NotificationHitKind::Content ||
          target.kind == State::NotificationHitKind::Action) {
        state->notifications->pressed_target = target;
        SetCapture(hwnd);
        request_repaint(*state);
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      handle_click_release(*state, hit_test_notifications(*state, point));
      return 0;
    }

    case WM_TIMER:
      if (wParam == Constants::ANIMATION_TIMER_ID) {
        update_notifications(*state);
        return 0;
      }
      break;

    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE: {
      update_host_bounds(*state);
      relayout_notifications(*state, std::chrono::steady_clock::now());
      request_repaint(*state);
      return 0;
    }

    case WM_NCDESTROY:
      cleanup_render_surface(*state);
      state->notifications->host_hwnd = nullptr;
      state->notifications->animation_timer_active = false;
      return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace Features::Notifications
