#include "ui/context_menu/message_handler.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/windowsx.hpp"

#include "core/state/app_state.hpp"
#include "ui/context_menu/context_menu.hpp"
#include "ui/context_menu/interaction.hpp"
#include "ui/context_menu/layout.hpp"
#include "ui/context_menu/painter.hpp"
#include "ui/context_menu/render_context.hpp"
#include "ui/context_menu/state.hpp"
#include "ui/context_menu/types.hpp"
#include "utils/logger/logger.hpp"

namespace ui::context_menu::message_handler {

// 三次缓出曲线：t=0→0, t=1→1, 前段快后段慢
auto ease_out_cubic(float t) -> float {
  const float ft = 1.0f - t;
  return 1.0f - ft * ft * ft;
}

// 推进一帧：用已过时间 / 总时长算出 opacity，到期则停用
auto update_open_animation(ui::context_menu::MenuOpenAnimation& animation,
                           std::chrono::steady_clock::time_point now) -> bool {
  if (!animation.active) {
    return false;
  }

  const auto elapsed = now - animation.start_time;
  const auto elapsed_ms =
      static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
  const auto duration_ms =
      static_cast<float>(std::max<std::int64_t>(1, animation.duration.count()));
  const float raw_progress = std::clamp(elapsed_ms / duration_ms, 0.0f, 1.0f);
  const float eased = ease_out_cubic(raw_progress);

  animation.opacity = eased;

  if (raw_progress >= 1.0f) {
    animation.active = false;
    animation.opacity = 1.0f;
  }

  return true;
}

auto get_timer_owner_hwnd(const ContextMenuState& menu_state, HWND fallback) -> HWND {
  return menu_state.hwnd ? menu_state.hwnd : fallback;
}

auto get_submenu_item_at_point(core::AppState& state, const POINT& pt) -> int;
auto handle_paint(core::AppState& state, HWND hwnd) -> LRESULT;
auto handle_size(core::AppState& state, HWND hwnd) -> LRESULT;
auto handle_mouse_move(core::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam) -> LRESULT;
auto handle_mouse_leave(core::AppState& state, HWND hwnd) -> LRESULT;
auto handle_left_button_down(core::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_key_down(core::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam) -> LRESULT;
auto handle_kill_focus(core::AppState& state, HWND hwnd) -> LRESULT;
auto handle_open_animation_timer(core::AppState& state, HWND hwnd) -> LRESULT;
auto handle_timer(core::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT;
auto handle_destroy(core::AppState& state, HWND hwnd) -> LRESULT;

auto window_procedure(core::AppState& state, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  switch (msg) {
    case WM_PAINT:
      return handle_paint(state, hwnd);
    case WM_SIZE:
      return handle_size(state, hwnd);
    case WM_MOUSEMOVE:
      return handle_mouse_move(state, hwnd, wParam, lParam);
    case WM_MOUSELEAVE:
      return handle_mouse_leave(state, hwnd);
    case WM_LBUTTONDOWN:
      return handle_left_button_down(state, hwnd, wParam, lParam);
    case WM_KEYDOWN:
      return handle_key_down(state, hwnd, wParam, lParam);
    case WM_KILLFOCUS:
      return handle_kill_focus(state, hwnd);
    case WM_TIMER:
      return handle_timer(state, hwnd, wParam);
    case WM_DESTROY:
      return handle_destroy(state, hwnd);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Win32 回调入口：从 GWLP_USERDATA 取出 AppState 并分派
auto CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
  core::AppState* app_state = nullptr;

  if (msg == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lParam);
    app_state = static_cast<core::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_state));
  } else {
    app_state = reinterpret_cast<core::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (app_state) {
    return window_procedure(*app_state, hwnd, msg, wParam, lParam);
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

auto get_submenu_item_at_point(core::AppState& state, const POINT& pt) -> int {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();
  int current_y = layout.padding;
  for (size_t i = 0; i < current_submenu.size(); ++i) {
    const auto& item = current_submenu[i];
    int item_height = (item.type == ui::context_menu::MenuItemType::Separator)
                          ? layout.separator_height
                          : layout.item_height;
    if (pt.y >= current_y && pt.y < current_y + item_height) {
      return static_cast<int>(i);
    }
    current_y += item_height;
  }
  return -1;
}

auto handle_paint(core::AppState& state, HWND hwnd) -> LRESULT {
  const auto& menu_state = *state.context_menu;
  PAINTSTRUCT ps{};
  if (BeginPaint(hwnd, &ps)) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    if (hwnd == menu_state.submenu_hwnd) {
      ui::context_menu::painter::paint_submenu(state, rect);
    } else {
      ui::context_menu::painter::paint_context_menu(state, rect);
    }
    EndPaint(hwnd, &ps);
  }
  return 0;
}

auto handle_size(core::AppState& state, HWND hwnd) -> LRESULT {
  const auto& menu_state = *state.context_menu;
  RECT rc;
  GetClientRect(hwnd, &rc);
  SIZE new_size = {rc.right - rc.left, rc.bottom - rc.top};
  if (hwnd == menu_state.submenu_hwnd) {
    Logger().debug("Resizing submenu to size: {}x{}", new_size.cx, new_size.cy);
    if (ui::context_menu::render_context::resize_submenu(state, new_size)) {
      ui::context_menu::painter::paint_submenu(state, rc);
    }
  } else if (hwnd == menu_state.hwnd) {
    Logger().debug("Resizing context menu to size: {}x{}", new_size.cx, new_size.cy);
    if (ui::context_menu::render_context::resize_context_menu(state, new_size)) {
      ui::context_menu::painter::paint_context_menu(state, rc);
    }
  }
  return 0;
}

auto handle_mouse_move(core::AppState& state, HWND hwnd, WPARAM, LPARAM lParam) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  if (hwnd == menu_state.submenu_hwnd) {
    int submenu_hover_index = get_submenu_item_at_point(state, pt);
    if (ui::context_menu::interaction::on_submenu_mouse_move(state, submenu_hover_index,
                                                             timer_owner)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  } else {
    int hover_index = ui::context_menu::layout::get_menu_item_at_point(state, pt);
    if (ui::context_menu::interaction::on_main_mouse_move(state, hover_index, timer_owner)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
  TrackMouseEvent(&tme);
  return 0;
}

auto handle_mouse_leave(core::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  if (ui::context_menu::interaction::on_mouse_leave(state, hwnd, timer_owner)) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  return 0;
}

auto handle_left_button_down(core::AppState& state, HWND hwnd, WPARAM, LPARAM lParam) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  if (hwnd == menu_state.submenu_hwnd) {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int clicked_index = get_submenu_item_at_point(state, pt);
    const auto& current_submenu = menu_state.get_current_submenu();
    if (clicked_index >= 0 && clicked_index < static_cast<int>(current_submenu.size())) {
      const auto& item = current_submenu[clicked_index];
      if (item.type == ui::context_menu::MenuItemType::Normal && item.is_enabled) {
        ui::context_menu::handle_menu_action(state, item);
        DestroyWindow(menu_state.hwnd);  // Close on selection
      }
    }
  } else {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int clicked_index = ui::context_menu::layout::get_menu_item_at_point(state, pt);
    if (clicked_index >= 0 && clicked_index < static_cast<int>(menu_state.items.size())) {
      const auto& item = menu_state.items[clicked_index];
      if (item.type == ui::context_menu::MenuItemType::Normal && item.is_enabled) {
        if (item.has_submenu()) {
          ui::context_menu::interaction::cancel_pending_intent(state, timer_owner);
          ui::context_menu::show_submenu(state, clicked_index);
          InvalidateRect(menu_state.hwnd, nullptr, FALSE);
        } else {
          ui::context_menu::handle_menu_action(state, item);
          DestroyWindow(menu_state.hwnd);  // Close on selection
        }
      }
    }
  }
  return 0;
}

auto handle_key_down(core::AppState& state, HWND hwnd, WPARAM wParam, LPARAM) -> LRESULT {
  switch (wParam) {
    case VK_ESCAPE:
      DestroyWindow(hwnd);
      break;
  }
  return 0;
}

auto handle_kill_focus(core::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  HWND new_focus = GetFocus();
  if (new_focus != nullptr &&
      (new_focus == menu_state.hwnd || new_focus == menu_state.submenu_hwnd)) {
    return 0;
  }

  Logger().debug("Menu lost focus to external window, hiding entire menu system");
  ui::context_menu::interaction::cancel_pending_intent(state, timer_owner);
  ui::context_menu::hide_and_destroy_menu(state);
  return 0;
}

// 动画帧回调：推进主菜单/子菜单动画并重绘，全部结束后关定时器
auto handle_open_animation_timer(core::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const auto now = std::chrono::steady_clock::now();
  const bool main_changed = update_open_animation(menu_state.main_animation, now);
  const bool submenu_changed = update_open_animation(menu_state.submenu_animation, now);

  if (main_changed && menu_state.hwnd) {
    RECT rect{};
    GetClientRect(menu_state.hwnd, &rect);
    ui::context_menu::painter::paint_context_menu(state, rect);
  }

  if (submenu_changed && menu_state.submenu_hwnd) {
    RECT rect{};
    GetClientRect(menu_state.submenu_hwnd, &rect);
    ui::context_menu::painter::paint_submenu(state, rect);
  }

  if (!menu_state.main_animation.active && !menu_state.submenu_animation.active) {
    KillTimer(get_timer_owner_hwnd(menu_state, hwnd), ui::context_menu::OPEN_ANIMATION_TIMER_ID);
  }

  return 0;
}

auto handle_timer(core::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  // 动画定时器与交互定时器 ID 不同，分开处理
  if (timer_id == ui::context_menu::OPEN_ANIMATION_TIMER_ID) {
    return handle_open_animation_timer(state, hwnd);
  }

  const auto action = ui::context_menu::interaction::on_timer(state, timer_owner, timer_id);
  switch (action.type) {
    case ui::context_menu::interaction::TimerActionType::ShowSubmenu:
      ui::context_menu::show_submenu(state, action.parent_index);
      break;
    case ui::context_menu::interaction::TimerActionType::HideSubmenu:
      ui::context_menu::hide_submenu(state);
      break;
    case ui::context_menu::interaction::TimerActionType::None:
    default:
      break;
  }

  if (action.invalidate_main && menu_state.hwnd) {
    InvalidateRect(menu_state.hwnd, nullptr, FALSE);
  }

  return 0;
}

auto handle_destroy(core::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  if (hwnd == menu_state.hwnd) {
    ui::context_menu::interaction::cancel_pending_intent(state, hwnd);
  }
  return 0;
}

}  // namespace ui::context_menu::message_handler
