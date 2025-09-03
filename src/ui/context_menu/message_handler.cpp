module;

#include <windows.h>
#include <windowsx.h>

#include <string>

module UI.ContextMenu.MessageHandler;

import std;
import Core.State;
import Core.Events;
import UI.ContextMenu;
import UI.ContextMenu.Layout;
import UI.ContextMenu.Painter;
import UI.ContextMenu.D2DContext;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import Utils.Logger;
import Utils.String;

namespace {

using UI::ContextMenu::State::ContextMenuState;

auto get_submenu_item_at_point(Core::State::AppState& state, const POINT& pt) -> int;
auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_key_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT;

auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
  switch (msg) {
    case WM_PAINT:
      return handle_paint(state, hwnd);
    case WM_SIZE:
      return handle_size(state, hwnd);
    case WM_MOUSEMOVE:
      return handle_mouse_move(state, hwnd, wParam, lParam);
    case WM_LBUTTONDOWN:
      return handle_left_button_down(state, hwnd, wParam, lParam);
    case WM_KEYDOWN:
      return handle_key_down(state, hwnd, wParam, lParam);
    case WM_KILLFOCUS:
      return handle_kill_focus(state, hwnd);
    case WM_TIMER:
      return handle_timer(state, hwnd, wParam);
    case WM_DESTROY:
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // anonymous namespace

namespace UI::ContextMenu::MessageHandler {

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

}  // namespace UI::ContextMenu::MessageHandler

namespace {

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
    UI::ContextMenu::D2DContext::resize_submenu(state, new_size);
  } else if (hwnd == menu_state.hwnd) {
    Logger().debug("Resizing context menu to size: {}x{}", new_size.cx, new_size.cy);
    UI::ContextMenu::D2DContext::resize_context_menu(state, new_size);
  }
  return 0;
}

auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM, LPARAM lParam) -> LRESULT {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  if (hwnd == menu_state.submenu_hwnd) {
    int submenu_hover_index = get_submenu_item_at_point(state, pt);
    if (submenu_hover_index != interaction.submenu_hover_index) {
      interaction.submenu_hover_index = submenu_hover_index;
      Logger().debug("Submenu hover index changed to: {}", submenu_hover_index);
      InvalidateRect(hwnd, nullptr, FALSE);
    }

    // 取消任何待处理的隐藏定时器
    if (interaction.hide_timer_id != 0) {
      Logger().debug("Killing hide timer");
      KillTimer(hwnd, interaction.HIDE_TIMER_ID);
      interaction.hide_timer_id = 0;
    }
  } else {
    // 处理主菜单鼠标移动
    int hover_index = UI::ContextMenu::Layout::get_menu_item_at_point(state, pt);

    if (hover_index != interaction.hover_index) {
      interaction.hover_index = hover_index;

      // 取消任何待处理的显示定时器
      if (interaction.show_timer_id != 0) {
        Logger().debug("Killing show timer");
        KillTimer(hwnd, interaction.SHOW_TIMER_ID);
        interaction.show_timer_id = 0;
        interaction.pending_submenu_index = -1;
      }

      // 检查是否需要延迟显示子菜单
      if (hover_index >= 0 && hover_index < static_cast<int>(menu_state.items.size())) {
        const auto& item = menu_state.items[hover_index];
        Logger().debug("Mouse moved over menu item: {} (has_submenu: {})",
                       Utils::String::ToUtf8(item.text), item.has_submenu());
        if (item.has_submenu()) {
          // 如果已经显示的是同一个子菜单，不需要重新显示
          if (menu_state.submenu_hwnd == nullptr ||
              menu_state.submenu_parent_index != hover_index) {
            Logger().debug("Setting timer to show submenu for item: {}",
                           Utils::String::ToUtf8(item.text));
            // 先隐藏当前子菜单
            UI::ContextMenu::hide_submenu(state);

            // 设置延迟显示定时器
            interaction.pending_submenu_index = hover_index;
            interaction.show_timer_id =
                SetTimer(hwnd, interaction.SHOW_TIMER_ID, interaction.SHOW_TIMER_DELAY, nullptr);
            Logger().debug("Show timer set with ID: {}", interaction.show_timer_id);
          } else {
            Logger().debug("Same submenu already shown, not setting timer");
          }
        } else {
          // 没有子菜单，启动延迟隐藏而不是立即隐藏
          // 这样可以处理对角线移动，但也能在用户明确移动到其他项时隐藏
          if (menu_state.submenu_hwnd != nullptr) {
            Logger().debug("Setting timer to hide submenu");
            if (interaction.hide_timer_id != 0) {
              KillTimer(hwnd, interaction.HIDE_TIMER_ID);
            }
            interaction.hide_timer_id =
                SetTimer(hwnd, UI::ContextMenu::Types::InteractionState::HIDE_TIMER_ID,
                         UI::ContextMenu::Types::InteractionState::HIDE_TIMER_DELAY, nullptr);
            Logger().debug("Hide timer set with ID: {}", interaction.hide_timer_id);
          }
        }
      } else {
        Logger().debug("Hover index out of bounds or invalid");
      }

      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  // 开始鼠标跟踪
  TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
  TrackMouseEvent(&tme);
  return 0;
}

auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM, LPARAM lParam)
    -> LRESULT {
  auto& menu_state = *state.context_menu;
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
    int hover_index = menu_state.interaction.hover_index;
    if (hover_index >= 0 && hover_index < static_cast<int>(menu_state.items.size())) {
      const auto& item = menu_state.items[hover_index];
      if (item.type == UI::ContextMenu::Types::MenuItemType::Normal && item.is_enabled) {
        if (item.has_submenu()) {
          UI::ContextMenu::show_submenu(state, hover_index);
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
      // Other key handling (VK_UP, VK_DOWN, etc.) would go here
  }
  return 0;
}

auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& menu_state = *state.context_menu;

  // 检查焦点是否转移到了菜单系统内的其他窗口
  HWND new_focus = GetFocus();
  if (new_focus != nullptr &&
      (new_focus == menu_state.hwnd || new_focus == menu_state.submenu_hwnd)) {
    return 0;
  }

  Logger().debug("Menu lost focus to external window, hiding entire menu system");
  UI::ContextMenu::hide_and_destroy_menu(state);
  return 0;
}

auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;

  if (timer_id == interaction.SHOW_TIMER_ID) {
    Logger().debug("Show timer expired, showing submenu for index: {}",
                   interaction.pending_submenu_index);
    // 清理显示定时器
    KillTimer(hwnd, interaction.SHOW_TIMER_ID);
    interaction.show_timer_id = 0;

    // 验证待显示的索引仍然有效且鼠标仍在该菜单项上
    if (interaction.pending_submenu_index >= 0 &&
        interaction.pending_submenu_index < static_cast<int>(menu_state.items.size())) {
      Logger().debug("Index is valid, checking if mouse is still over the item");
      if (interaction.pending_submenu_index == interaction.hover_index) {
        const auto& item = menu_state.items[interaction.pending_submenu_index];
        Logger().debug("Mouse is still over the item: {} (has_submenu: {})",
                       Utils::String::ToUtf8(item.text), item.has_submenu());
        if (item.has_submenu()) {
          UI::ContextMenu::show_submenu(state, interaction.pending_submenu_index);
        }
      }
    } else {
      Logger().debug("Index is invalid: {} (items size: {})", interaction.pending_submenu_index,
                     menu_state.items.size());
    }

    // 重置待显示索引
    interaction.pending_submenu_index = -1;
  } else if (timer_id == UI::ContextMenu::Types::InteractionState::HIDE_TIMER_ID) {
    Logger().debug("Hide timer expired, hiding submenu");
    // 清理隐藏定时器
    KillTimer(hwnd, interaction.HIDE_TIMER_ID);
    interaction.hide_timer_id = 0;

    // 隐藏子菜单
    UI::ContextMenu::hide_submenu(state);
  }
  return 0;
}

}  // anonymous namespace