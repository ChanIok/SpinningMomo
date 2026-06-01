module;

module UI.NotificationWindow;

import std;
import Core.State;
import Core.Notifications.Types;
import UI.NotificationWindow.Painter;
import UI.NotificationWindow.RenderContext;
import UI.NotificationWindow.State;
import UI.NotificationWindow.Types;
import UI.FloatingWindow.State;
import Utils.Display;
import Utils.Logger;
import <dwmapi.h>;
import <windows.h>;

namespace UI::NotificationWindow::MessageHandler {
LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
}

namespace UI::NotificationWindow {

auto ease_out_cubic(float t) -> float {
  const float ft = 1.0f - t;
  return 1.0f - ft * ft * ft;
}

auto slide_progress(std::chrono::steady_clock::duration elapsed) -> float {
  return std::min(
      1.0f,
      static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
          NotificationWindow::SLIDE_DURATION.count());
}

auto lerp_pos(POINT start, POINT target, float eased) -> POINT {
  return {start.x + static_cast<int>((target.x - start.x) * eased),
          start.y + static_cast<int>((target.y - start.y) * eased)};
}

auto hit_targets_equal(const NotificationHitTarget& left, const NotificationHitTarget& right)
    -> bool {
  return left.kind == right.kind && left.notification_id == right.notification_id;
}

auto rect_contains(const RECT& rect, const POINT& point) -> bool {
  return point.x >= rect.left && point.x < rect.right && point.y >= rect.top &&
         point.y < rect.bottom;
}

auto register_host_window_class(HINSTANCE instance) -> bool {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = MessageHandler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = NotificationWindow::NOTIFICATION_WINDOW_CLASS.c_str();
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  if (!RegisterClassExW(&wc)) {
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  registered = true;
  return true;
}

auto get_notification_work_area(const Core::State::AppState& state) -> RECT {
  const auto& floating_window = *state.floating_window;
  auto monitor = Utils::Display::get_working_monitor(floating_window.window.hwnd,
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

auto ensure_host_window(Core::State::AppState& state) -> bool {
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

  window_state.dpi = Painter::get_current_dpi(state);
  const SIZE host_size = Painter::get_host_size(window_state.dpi);
  const RECT work_area = get_notification_work_area(state);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              NotificationWindow::NOTIFICATION_WINDOW_CLASS.c_str(),
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

auto start_animation_timer(Core::State::AppState& state) -> void {
  auto& window_state = *state.notification_window;
  if (!window_state.animation_timer_active && window_state.host_hwnd) {
    SetTimer(window_state.host_hwnd, NotificationWindow::ANIMATION_TIMER_ID,
             NotificationWindow::ANIMATION_FRAME_INTERVAL, nullptr);
    window_state.animation_timer_active = true;
  }
}

auto stop_animation_timer(Core::State::AppState& state) -> void {
  auto& window_state = *state.notification_window;
  if (window_state.animation_timer_active && window_state.host_hwnd) {
    KillTimer(window_state.host_hwnd, NotificationWindow::ANIMATION_TIMER_ID);
  }
  window_state.animation_timer_active = false;
}

auto mark_notification_fading_out(Notification& notification,
                                  std::chrono::steady_clock::time_point now) -> void {
  if (notification.state == NotificationAnimState::FadingOut ||
      notification.state == NotificationAnimState::Done) {
    return;
  }

  notification.state = NotificationAnimState::FadingOut;
  notification.last_state_change_time = now;
  notification.animation_start_pos = notification.current_pos;
  notification.animation_start_opacity = notification.opacity;
  notification.is_hovered = false;
  notification.action_hovered = false;
}

auto active_layout_count(const Core::State::AppState& state) -> size_t {
  size_t count = 0;
  for (const auto& notification : state.notification_window->active_notifications) {
    if (!Painter::is_exiting(notification.state)) {
      ++count;
    }
  }
  return count;
}

auto update_host_bounds(Core::State::AppState& state) -> void {
  auto& window_state = *state.notification_window;
  if (!window_state.host_hwnd) {
    return;
  }

  const RECT work_area = get_notification_work_area(state);
  window_state.dpi = Painter::get_current_dpi(state);
  const SIZE host_size = Painter::get_host_size(window_state.dpi);
  const POINT host_position{work_area.right - host_size.cx, work_area.bottom - host_size.cy};

  if (window_state.host_size.cx != host_size.cx || window_state.host_size.cy != host_size.cy ||
      window_state.host_position.x != host_position.x ||
      window_state.host_position.y != host_position.y) {
    SetWindowPos(window_state.host_hwnd, HWND_TOPMOST, host_position.x, host_position.y,
                 host_size.cx, host_size.cy, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    window_state.host_size = host_size;
    window_state.host_position = host_position;
  }
}

auto relayout_notifications(Core::State::AppState& state, std::chrono::steady_clock::time_point now)
    -> void {
  const int dpi = Painter::get_current_dpi(state);
  const int margin = Painter::get_layout_margin(dpi);
  const int spacing = Painter::scale_for_dpi(NotificationWindow::BASE_SPACING, dpi);
  const int target_x = margin;
  int current_y = state.notification_window->host_size.cy - margin;

  for (auto it = state.notification_window->active_notifications.rbegin();
       it != state.notification_window->active_notifications.rend(); ++it) {
    auto& notification = *it;
    if (Painter::is_exiting(notification.state)) {
      continue;
    }

    current_y -= notification.height;
    const POINT new_target{target_x, current_y};

    if (notification.state == NotificationAnimState::Spawning) {
      notification.target_pos = new_target;
      notification.current_pos = {state.notification_window->host_size.cx, new_target.y};
      notification.animation_start_pos = notification.current_pos;
      notification.animation_start_opacity = 0.0f;
      notification.opacity = 0.0f;
      notification.state = NotificationAnimState::SlidingIn;
      notification.last_state_change_time = now;
    } else if (notification.target_pos.y != new_target.y ||
               notification.target_pos.x != new_target.x) {
      notification.target_pos = new_target;
      if (notification.state == NotificationAnimState::Displaying ||
          notification.state == NotificationAnimState::MovingUp) {
        notification.animation_start_pos = notification.current_pos;
        notification.animation_start_opacity = notification.opacity;
        notification.state = NotificationAnimState::MovingUp;
        notification.last_state_change_time = now;
      }
    }

    current_y -= spacing;
  }

  Painter::update_all_notification_rects(state);
}

auto show_host(Core::State::AppState& state) -> void {
  if (!state.notification_window->host_hwnd) {
    return;
  }

  update_host_bounds(state);
  ShowWindow(state.notification_window->host_hwnd, SW_SHOWNA);
}

auto hide_host_if_idle(Core::State::AppState& state) -> void {
  if (!state.notification_window->active_notifications.empty()) {
    return;
  }

  stop_animation_timer(state);
  state.notification_window->hover_target = {};
  state.notification_window->pressed_target = {};
  if (state.notification_window->host_hwnd) {
    ShowWindow(state.notification_window->host_hwnd, SW_HIDE);
  }
}

auto find_notification(Core::State::AppState& state, size_t id)
    -> std::list<Notification>::iterator {
  return std::ranges::find_if(
      state.notification_window->active_notifications,
      [id](const Notification& notification) { return notification.id == id; });
}

auto hit_test_notifications(Core::State::AppState& state, POINT point) -> NotificationHitTarget {
  if (state.notification_window->active_notifications.empty()) {
    return {};
  }

  auto& notifications = state.notification_window->active_notifications;
  for (auto it = notifications.rbegin(); it != notifications.rend(); ++it) {
    const auto& notification = *it;
    if (Painter::is_exiting(notification.state) || notification.opacity <= 0.05f) {
      continue;
    }

    if (notification.action && rect_contains(notification.action_rect, point)) {
      return {.kind = NotificationHitKind::Action, .notification_id = notification.id};
    }
    if (rect_contains(notification.content_rect, point)) {
      return {.kind = NotificationHitKind::Content, .notification_id = notification.id};
    }
    if (rect_contains(notification.card_rect, point)) {
      return {.kind = NotificationHitKind::Card, .notification_id = notification.id};
    }
  }

  return {};
}

auto update_hover_state(Core::State::AppState& state, NotificationHitTarget target) -> bool {
  const bool target_changed = !hit_targets_equal(state.notification_window->hover_target, target);
  state.notification_window->hover_target = target;

  bool changed = target_changed;
  const auto now = std::chrono::steady_clock::now();
  for (auto& notification : state.notification_window->active_notifications) {
    const bool hovered =
        target.kind != NotificationHitKind::None && target.notification_id == notification.id;
    if (notification.is_hovered != hovered) {
      notification.is_hovered = hovered;
      changed = true;

      if (hovered && notification.state == NotificationAnimState::Displaying) {
        notification.pause_start_time = now;
      } else if (!hovered && notification.state == NotificationAnimState::Displaying &&
                 notification.pause_start_time.time_since_epoch().count() > 0) {
        const auto paused_duration = now - notification.pause_start_time;
        notification.total_paused_duration +=
            std::chrono::duration_cast<std::chrono::milliseconds>(paused_duration);
        notification.pause_start_time = {};
      }
    }

    const bool action_hovered =
        target.kind == NotificationHitKind::Action && target.notification_id == notification.id;
    if (notification.action_hovered != action_hovered) {
      notification.action_hovered = action_hovered;
      changed = true;
    }
  }

  return changed;
}

auto update_notification_animation(Notification& notification,
                                   std::chrono::steady_clock::time_point now) -> bool {
  auto elapsed_time = now - notification.last_state_change_time;
  if (notification.state == NotificationAnimState::Displaying && notification.is_hovered) {
    return false;
  }
  if (notification.state == NotificationAnimState::Displaying) {
    elapsed_time -= notification.total_paused_duration;
  }

  switch (notification.state) {
    case NotificationAnimState::SlidingIn: {
      const float p = slide_progress(elapsed_time);
      const float eased = ease_out_cubic(p);
      notification.current_pos =
          lerp_pos(notification.animation_start_pos, notification.target_pos, eased);
      notification.opacity = eased;
      if (p >= 1.0f) {
        notification.state = NotificationAnimState::Displaying;
        notification.last_state_change_time = now;
        notification.display_started_time = now;
        notification.current_pos = notification.target_pos;
        notification.opacity = 1.0f;
        notification.total_paused_duration = std::chrono::milliseconds(0);
        notification.pause_start_time = {};
      }
      return true;
    }

    case NotificationAnimState::Displaying:
      if (!notification.is_hovered && elapsed_time >= notification.duration) {
        mark_notification_fading_out(notification, now);
        return true;
      }
      return false;

    case NotificationAnimState::MovingUp: {
      const float p = slide_progress(elapsed_time);
      notification.current_pos =
          lerp_pos(notification.animation_start_pos, notification.target_pos, ease_out_cubic(p));
      if (p >= 1.0f) {
        notification.state = NotificationAnimState::Displaying;
        notification.last_state_change_time = now;
        notification.current_pos = notification.target_pos;
      }
      return true;
    }

    case NotificationAnimState::FadingOut: {
      const float progress = std::min(
          1.0f, static_cast<float>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()) /
                    NotificationWindow::FADE_DURATION.count());
      notification.opacity =
          notification.animation_start_opacity * (1.0f - ease_out_cubic(progress));
      if (progress >= 1.0f) {
        notification.state = NotificationAnimState::Done;
      }
      return true;
    }

    case NotificationAnimState::Spawning:
    case NotificationAnimState::Done:
      return false;
  }

  return false;
}

auto execute_action_callback(Core::State::AppState& state, const NotificationHitTarget& target)
    -> void {
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

auto handle_click_release(Core::State::AppState& state, NotificationHitTarget release_target)
    -> void {
  const auto pressed_target = state.notification_window->pressed_target;
  state.notification_window->pressed_target = {};
  if (!hit_targets_equal(pressed_target, release_target)) {
    request_repaint(state);
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (release_target.kind == NotificationHitKind::Action) {
    execute_action_callback(state, release_target);
  }

  if (release_target.kind == NotificationHitKind::Content ||
      release_target.kind == NotificationHitKind::Action) {
    auto it = find_notification(state, release_target.notification_id);
    if (it != state.notification_window->active_notifications.end()) {
      mark_notification_fading_out(*it, now);
      relayout_notifications(state, now);
    }
  }

  request_repaint(state);
}

auto update_notifications(Core::State::AppState& state) -> void {
  if (!state.notification_window->host_hwnd) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  bool needs_repaint = false;
  bool erased = false;

  for (auto it = state.notification_window->active_notifications.begin();
       it != state.notification_window->active_notifications.end();) {
    auto& notification = *it;
    needs_repaint = update_notification_animation(notification, now) || needs_repaint;

    if (notification.state == NotificationAnimState::Done) {
      it = state.notification_window->active_notifications.erase(it);
      erased = true;
    } else {
      ++it;
    }
  }

  if (erased) {
    relayout_notifications(state, now);
    needs_repaint = true;
  } else if (needs_repaint) {
    Painter::update_all_notification_rects(state);
  }

  if (state.notification_window->active_notifications.empty()) {
    hide_host_if_idle(state);
    return;
  }

  if (needs_repaint) {
    Painter::paint_notifications(state);
  }
}

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
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

auto cleanup(Core::State::AppState& state) -> void {
  stop_animation_timer(state);

  if (state.notification_window->host_hwnd) {
    DestroyWindow(state.notification_window->host_hwnd);
    state.notification_window->host_hwnd = nullptr;
  } else {
    UI::NotificationWindow::RenderContext::cleanup_render_context(state);
  }

  state.notification_window->active_notifications.clear();
  state.notification_window->hover_target = {};
  state.notification_window->pressed_target = {};
  state.notification_window->host_size = {};
  state.notification_window->host_position = {};
  state.notification_window->dpi = 96;
  state.notification_window->next_id = 0;
}

auto show_notification(Core::State::AppState& state,
                       Core::Notifications::Types::NotificationOptions options) -> void {
  if (!ensure_host_window(state)) {
    return;
  }
  if (!UI::NotificationWindow::RenderContext::ensure_render_context(state)) {
    Logger().error("Failed to initialize notification render context");
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (active_layout_count(state) >=
      static_cast<size_t>(NotificationWindow::MAX_VISIBLE_NOTIFICATIONS)) {
    for (auto& notification : state.notification_window->active_notifications) {
      if (!Painter::is_exiting(notification.state)) {
        mark_notification_fading_out(notification, now);
        break;
      }
    }
  }

  const int dpi = Painter::get_current_dpi(state);
  const int card_width = Painter::get_window_width(dpi);
  const auto action = Painter::normalize_action(std::move(options.action));
  const auto layout =
      Painter::compute_notification_layout(state, options.message, action, card_width);

  Notification notification{
      .id = state.notification_window->next_id++,
      .title = std::move(options.title),
      .message = std::move(options.message),
      .action = action,
      .colors = Painter::resolve_notification_theme_colors(state),
      .state = NotificationAnimState::Spawning,
      .last_state_change_time = now,
      .duration = options.duration,
  };
  notification.width = card_width;
  notification.layout = layout;
  notification.height = Painter::measure_card_height(layout, dpi);

  state.notification_window->active_notifications.emplace_back(std::move(notification));
  update_host_bounds(state);
  relayout_notifications(state, now);
  show_host(state);
  start_animation_timer(state);
  Painter::paint_notifications(state);
}

auto request_repaint(Core::State::AppState& state) -> void { Painter::request_repaint(state); }

}  // namespace UI::NotificationWindow
