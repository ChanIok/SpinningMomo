module;

module UI.ContextMenu.MessageHandler;

import std;
import Core.State;
import UI.ContextMenu;
import UI.ContextMenu.Layout;
import UI.ContextMenu.Painter;
import UI.ContextMenu.RenderContext;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Interaction;
import Utils.Logger;
import <windows.h>;
import <windowsx.h>;

namespace UI::ContextMenu::MessageHandler {

// 三次缓出曲线：t=0→0, t=1→1, 前段快后段慢
auto ease_out_cubic(float t) -> float {
  const float ft = 1.0f - t;
  return 1.0f - ft * ft * ft;
}

// 推进一帧：用已过时间 / 总时长算出 opacity，到期则停用
auto update_open_animation(UI::ContextMenu::Types::MenuOpenAnimation& animation,
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

auto get_timer_owner_hwnd(const State::ContextMenuState& menu_state, HWND fallback) -> HWND {
  return menu_state.hwnd ? menu_state.hwnd : fallback;
}

auto get_submenu_item_at_point(Core::State::AppState& state, const POINT& pt) -> int;
auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_mouse_leave(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_key_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_open_animation_timer(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT;
auto handle_destroy(Core::State::AppState& state, HWND hwnd) -> LRESULT;

auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
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
  Core::State::AppState* app_state = nullptr;

  if (msg == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lParam);
    app_state = static_cast<Core::State::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_state));
  } else {
    app_state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (app_state) {
    return window_procedure(*app_state, hwnd, msg, wParam, lParam);
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

auto get_submenu_item_at_point(Core::State::AppState& state, const POINT& pt) -> int {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();
  int current_y = layout.padding;
  for (size_t i = 0; i < current_submenu.size(); ++i) {
    const auto& item = current_submenu[i];
    int item_height = (item.type == UI::ContextMenu::Types::MenuItemType::Separator)
                          ? layout.separator_height
                          : layout.item_height;
    if (pt.y >= current_y && pt.y < current_y + item_height) {
      return static_cast<int>(i);
    }
    current_y += item_height;
  }
  return -1;
}

auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  const auto& menu_state = *state.context_menu;
  PAINTSTRUCT ps{};
  if (BeginPaint(hwnd, &ps)) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    if (hwnd == menu_state.submenu_hwnd) {
      UI::ContextMenu::Painter::paint_submenu(state, rect);
    } else {
      UI::ContextMenu::Painter::paint_context_menu(state, rect);
    }
    EndPaint(hwnd, &ps);
  }
  return 0;
}

auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  const auto& menu_state = *state.context_menu;
  RECT rc;
  GetClientRect(hwnd, &rc);
  SIZE new_size = {rc.right - rc.left, rc.bottom - rc.top};
  if (hwnd == menu_state.submenu_hwnd) {
    Logger().debug("Resizing submenu to size: {}x{}", new_size.cx, new_size.cy);
    if (UI::ContextMenu::RenderContext::resize_submenu(state, new_size)) {
      UI::ContextMenu::Painter::paint_submenu(state, rc);
    }
  } else if (hwnd == menu_state.hwnd) {
    Logger().debug("Resizing context menu to size: {}x{}", new_size.cx, new_size.cy);
    if (UI::ContextMenu::RenderContext::resize_context_menu(state, new_size)) {
      UI::ContextMenu::Painter::paint_context_menu(state, rc);
    }
  }
  return 0;
}

auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM, LPARAM lParam) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  if (hwnd == menu_state.submenu_hwnd) {
    int submenu_hover_index = get_submenu_item_at_point(state, pt);
    if (UI::ContextMenu::Interaction::on_submenu_mouse_move(state, submenu_hover_index,
                                                            timer_owner)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  } else {
    int hover_index = UI::ContextMenu::Layout::get_menu_item_at_point(state, pt);
    if (UI::ContextMenu::Interaction::on_main_mouse_move(state, hover_index, timer_owner)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
  TrackMouseEvent(&tme);
  return 0;
}

auto handle_mouse_leave(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  if (UI::ContextMenu::Interaction::on_mouse_leave(state, hwnd, timer_owner)) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  return 0;
}

auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM, LPARAM lParam)
    -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  if (hwnd == menu_state.submenu_hwnd) {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int clicked_index = get_submenu_item_at_point(state, pt);
    const auto& current_submenu = menu_state.get_current_submenu();
    if (clicked_index >= 0 && clicked_index < static_cast<int>(current_submenu.size())) {
      const auto& item = current_submenu[clicked_index];
      if (item.type == UI::ContextMenu::Types::MenuItemType::Normal && item.is_enabled) {
        UI::ContextMenu::handle_menu_action(state, item);
        DestroyWindow(menu_state.hwnd);  // Close on selection
      }
    }
  } else {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int clicked_index = UI::ContextMenu::Layout::get_menu_item_at_point(state, pt);
    if (clicked_index >= 0 && clicked_index < static_cast<int>(menu_state.items.size())) {
      const auto& item = menu_state.items[clicked_index];
      if (item.type == UI::ContextMenu::Types::MenuItemType::Normal && item.is_enabled) {
        if (item.has_submenu()) {
          UI::ContextMenu::Interaction::cancel_pending_intent(state, timer_owner);
          UI::ContextMenu::show_submenu(state, clicked_index);
          InvalidateRect(menu_state.hwnd, nullptr, FALSE);
        } else {
          UI::ContextMenu::handle_menu_action(state, item);
          DestroyWindow(menu_state.hwnd);  // Close on selection
        }
      }
    }
  }
  return 0;
}

auto handle_key_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM) -> LRESULT {
  switch (wParam) {
    case VK_ESCAPE:
      DestroyWindow(hwnd);
      break;
  }
  return 0;
}

auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  HWND new_focus = GetFocus();
  if (new_focus != nullptr &&
      (new_focus == menu_state.hwnd || new_focus == menu_state.submenu_hwnd)) {
    return 0;
  }

  Logger().debug("Menu lost focus to external window, hiding entire menu system");
  UI::ContextMenu::Interaction::cancel_pending_intent(state, timer_owner);
  UI::ContextMenu::hide_and_destroy_menu(state);
  return 0;
}

// 动画帧回调：推进主菜单/子菜单动画并重绘，全部结束后关定时器
auto handle_open_animation_timer(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const auto now = std::chrono::steady_clock::now();
  const bool main_changed = update_open_animation(menu_state.main_animation, now);
  const bool submenu_changed = update_open_animation(menu_state.submenu_animation, now);

  if (main_changed && menu_state.hwnd) {
    RECT rect{};
    GetClientRect(menu_state.hwnd, &rect);
    UI::ContextMenu::Painter::paint_context_menu(state, rect);
  }

  if (submenu_changed && menu_state.submenu_hwnd) {
    RECT rect{};
    GetClientRect(menu_state.submenu_hwnd, &rect);
    UI::ContextMenu::Painter::paint_submenu(state, rect);
  }

  if (!menu_state.main_animation.active && !menu_state.submenu_animation.active) {
    KillTimer(get_timer_owner_hwnd(menu_state, hwnd),
              UI::ContextMenu::Types::OPEN_ANIMATION_TIMER_ID);
  }

  return 0;
}

auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT {
  auto& menu_state = *state.context_menu;
  const HWND timer_owner = get_timer_owner_hwnd(menu_state, hwnd);

  // 动画定时器与交互定时器 ID 不同，分开处理
  if (timer_id == UI::ContextMenu::Types::OPEN_ANIMATION_TIMER_ID) {
    return handle_open_animation_timer(state, hwnd);
  }

  const auto action = UI::ContextMenu::Interaction::on_timer(state, timer_owner, timer_id);
  switch (action.type) {
    case UI::ContextMenu::Interaction::TimerActionType::ShowSubmenu:
      UI::ContextMenu::show_submenu(state, action.parent_index);
      break;
    case UI::ContextMenu::Interaction::TimerActionType::HideSubmenu:
      UI::ContextMenu::hide_submenu(state);
      break;
    case UI::ContextMenu::Interaction::TimerActionType::None:
    default:
      break;
  }

  if (action.invalidate_main && menu_state.hwnd) {
    InvalidateRect(menu_state.hwnd, nullptr, FALSE);
  }

  return 0;
}

auto handle_destroy(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;
  if (hwnd == menu_state.hwnd) {
    UI::ContextMenu::Interaction::cancel_pending_intent(state, hwnd);
  }
  return 0;
}

}  // namespace UI::ContextMenu::MessageHandler
