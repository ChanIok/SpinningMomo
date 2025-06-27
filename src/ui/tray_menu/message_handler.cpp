module;

#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.TrayMenu.MessageHandler;

import std;
import Core.State;
import UI.TrayMenu;
import UI.TrayMenu.Layout;
import UI.TrayMenu.Painter;
import UI.TrayMenu.D2DContext;
import UI.TrayMenu.State;
import Utils.Logger;

namespace {

// 获取子菜单中指定点击位置的菜单项索引
auto get_submenu_item_at_point(const Core::State::AppState& state, const POINT& pt) -> int {
  const auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  int current_y = layout.padding;

  for (size_t i = 0; i < tray_menu.current_submenu.size(); ++i) {
    const auto& item = tray_menu.current_submenu[i];

    int item_height;
    if (item.type == UI::TrayMenu::State::MenuItemType::Separator) {
      item_height = layout.separator_height;
    } else {
      item_height = layout.item_height;
    }

    if (pt.y >= current_y && pt.y < current_y + item_height) {
      return static_cast<int>(i);
    }

    current_y += item_height;
  }

  return -1;
}

// 主窗口过程函数，负责将Windows消息翻译成应用程序事件
auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
  switch (msg) {
    case WM_PAINT:
      return UI::TrayMenu::MessageHandler::handle_paint(state, hwnd);

    case WM_SIZE:
      return UI::TrayMenu::MessageHandler::handle_size(state, hwnd);

    case WM_MOUSEMOVE:
      return UI::TrayMenu::MessageHandler::handle_mouse_move(state, hwnd, wParam, lParam);

    case WM_LBUTTONDOWN:
      return UI::TrayMenu::MessageHandler::handle_left_button_down(state, hwnd, wParam, lParam);

    case WM_KEYDOWN:
      return UI::TrayMenu::MessageHandler::handle_key_down(state, hwnd, wParam, lParam);

    case WM_KILLFOCUS:
      return UI::TrayMenu::MessageHandler::handle_kill_focus(state, hwnd);

    case WM_TIMER:
      return UI::TrayMenu::MessageHandler::handle_timer(state, hwnd, wParam);

    case WM_DESTROY: {
      // 区分主菜单和子菜单窗口的销毁
      if (hwnd == state.tray_menu.hwnd) {
        // 清理所有定时器
        auto& interaction = state.tray_menu.interaction;

        if (interaction.show_timer_id != 0) {
          KillTimer(hwnd, interaction.show_timer_id);
          interaction.show_timer_id = 0;
          interaction.pending_submenu_index = -1;
        }

        // 清理隐藏定时器
        if (interaction.hide_timer_id != 0) {
          KillTimer(hwnd, interaction.hide_timer_id);
          interaction.hide_timer_id = 0;
        }

        state.tray_menu.hwnd = nullptr;
        state.tray_menu.is_created = false;
        Logger().debug("Main menu window destroyed and all timers cleaned up");
      } else if (hwnd == state.tray_menu.submenu_hwnd) {
        state.tray_menu.submenu_hwnd = nullptr;
        Logger().debug("Submenu window destroyed");
      }
      return 0;
    }

    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

}  // anonymous namespace

namespace UI::TrayMenu::MessageHandler {

// 静态窗口过程函数，将窗口句柄与应用程序状态关联起来
auto CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
  Core::State::AppState* state = nullptr;

  if (msg == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lParam);
    state = static_cast<Core::State::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (!state) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  return window_procedure(*state, hwnd, msg, wParam, lParam);
}

// 处理WM_PAINT消息
auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  PAINTSTRUCT ps{};
  if (HDC hdc = BeginPaint(hwnd, &ps); hdc) {
    RECT rect{};
    GetClientRect(hwnd, &rect);

    // 检查是主菜单还是子菜单
    if (hwnd == state.tray_menu.submenu_hwnd) {
      UI::TrayMenu::Painter::paint_submenu(state, rect);
    } else {
      UI::TrayMenu::Painter::paint_tray_menu(state, rect);
    }

    EndPaint(hwnd, &ps);
  }
  return 0;
}

// 处理WM_SIZE消息
auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  // 获取新的窗口尺寸
  RECT rc;
  GetClientRect(hwnd, &rc);
  SIZE new_size = {rc.right - rc.left, rc.bottom - rc.top};

  // 根据窗口类型调整渲染目标大小
  if (hwnd == state.tray_menu.submenu_hwnd) {
    UI::TrayMenu::D2DContext::resize_submenu(state, new_size);
  } else {
    UI::TrayMenu::D2DContext::resize_main_menu(state, new_size);
  }

  return 0;
}

// 处理WM_MOUSEMOVE消息
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  auto& tray_menu = state.tray_menu;
  auto& interaction = tray_menu.interaction;
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  // 检查是主菜单还是子菜单
  if (hwnd == tray_menu.submenu_hwnd) {
    // 处理子菜单鼠标移动
    int submenu_hover_index = get_submenu_item_at_point(state, pt);
    if (submenu_hover_index != interaction.submenu_hover_index) {
      interaction.submenu_hover_index = submenu_hover_index;
      InvalidateRect(hwnd, nullptr, FALSE);
    }

    // 取消任何待处理的隐藏定时器
    if (interaction.hide_timer_id != 0) {
      KillTimer(hwnd, interaction.hide_timer_id);
      interaction.hide_timer_id = 0;
    }
  } else {
    // 处理主菜单鼠标移动
    int hover_index = UI::TrayMenu::Layout::get_menu_item_at_point(state, pt);

    if (hover_index != interaction.hover_index) {
      interaction.hover_index = hover_index;

      // 取消任何待处理的显示定时器
      if (interaction.show_timer_id != 0) {
        KillTimer(hwnd, interaction.show_timer_id);
        interaction.show_timer_id = 0;
        interaction.pending_submenu_index = -1;
      }

      // 检查是否需要延迟显示子菜单
      if (hover_index >= 0 && hover_index < static_cast<int>(tray_menu.items.size())) {
        const auto& item = tray_menu.items[hover_index];
        if (item.has_submenu()) {
          // 如果已经显示的是同一个子菜单，不需要重新显示
          if (tray_menu.submenu_hwnd == nullptr || tray_menu.submenu_parent_index != hover_index) {
            // 先隐藏当前子菜单
            Logger().debug("Hiding existing submenu before showing new one");
            UI::TrayMenu::hide_submenu(state);

            // 设置延迟显示定时器
            interaction.pending_submenu_index = hover_index;
            interaction.show_timer_id =
                SetTimer(hwnd, interaction.SHOW_TIMER_ID, interaction.SHOW_TIMER_DELAY, nullptr);
          }
        } else {
          // 没有子菜单，启动延迟隐藏而不是立即隐藏
          // 这样可以处理对角线移动，但也能在用户明确移动到其他项时隐藏
          if (tray_menu.submenu_hwnd != nullptr) {
            if (interaction.hide_timer_id != 0) {
              KillTimer(hwnd, interaction.hide_timer_id);
            }
            interaction.hide_timer_id =
                SetTimer(hwnd, UI::TrayMenu::State::InteractionState::HIDE_TIMER_ID,
                         UI::TrayMenu::State::InteractionState::HIDE_TIMER_DELAY, nullptr);
          }
        }
      }

      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  // 开始鼠标跟踪
  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd;
  TrackMouseEvent(&tme);

  return 0;
}

// 处理WM_LBUTTONDOWN消息
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  auto& tray_menu = state.tray_menu;

  // 检查是否点击的是子菜单
  if (hwnd == tray_menu.submenu_hwnd) {
    // 处理子菜单点击
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int clicked_index = get_submenu_item_at_point(state, pt);

    if (clicked_index >= 0 && clicked_index < static_cast<int>(tray_menu.current_submenu.size())) {
      const auto& item = tray_menu.current_submenu[clicked_index];
      if (item.type == UI::TrayMenu::State::MenuItemType::Normal && item.is_enabled) {
        UI::TrayMenu::handle_menu_command(state, item.command_id);
      }
    }
  } else {
    // 处理主菜单点击
    if (tray_menu.interaction.hover_index >= 0 &&
        tray_menu.interaction.hover_index < static_cast<int>(tray_menu.items.size())) {
      const auto& item = tray_menu.items[tray_menu.interaction.hover_index];
      if (item.type == UI::TrayMenu::State::MenuItemType::Normal && item.is_enabled) {
        if (item.has_submenu()) {
          // 如果有子菜单，显示子菜单
          UI::TrayMenu::show_submenu(state, tray_menu.interaction.hover_index);
        } else {
          // 否则执行命令
          UI::TrayMenu::handle_menu_command(state, item.command_id);
        }
      }
    }
  }
  return 0;
}

// 处理WM_KEYDOWN消息
auto handle_key_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  auto& tray_menu = state.tray_menu;
  switch (wParam) {
    case VK_ESCAPE:
      UI::TrayMenu::hide_menu(state);
      break;
    case VK_UP: {
      // 向上导航
      int new_index = tray_menu.interaction.hover_index - 1;
      while (new_index >= 0 &&
             tray_menu.items[new_index].type != UI::TrayMenu::State::MenuItemType::Normal) {
        new_index--;
      }
      if (new_index >= 0) {
        tray_menu.interaction.hover_index = new_index;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      break;
    }
    case VK_DOWN: {
      // 向下导航
      int new_index = tray_menu.interaction.hover_index + 1;
      while (new_index < static_cast<int>(tray_menu.items.size()) &&
             tray_menu.items[new_index].type != UI::TrayMenu::State::MenuItemType::Normal) {
        new_index++;
      }
      if (new_index < static_cast<int>(tray_menu.items.size())) {
        tray_menu.interaction.hover_index = new_index;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      break;
    }
    case VK_RIGHT: {
      // 进入子菜单
      if (tray_menu.interaction.hover_index >= 0 &&
          tray_menu.interaction.hover_index < static_cast<int>(tray_menu.items.size())) {
        const auto& item = tray_menu.items[tray_menu.interaction.hover_index];
        if (item.has_submenu()) {
          UI::TrayMenu::show_submenu(state, tray_menu.interaction.hover_index);
        }
      }
      break;
    }
    case VK_LEFT: {
      // 返回父菜单（隐藏子菜单）
      UI::TrayMenu::hide_submenu(state);
      break;
    }
    case VK_RETURN: {
      // 执行当前选中的菜单项
      if (tray_menu.interaction.hover_index >= 0 &&
          tray_menu.interaction.hover_index < static_cast<int>(tray_menu.items.size())) {
        const auto& item = tray_menu.items[tray_menu.interaction.hover_index];
        if (item.type == UI::TrayMenu::State::MenuItemType::Normal && item.is_enabled) {
          if (item.has_submenu()) {
            // 如果有子菜单，显示子菜单
            UI::TrayMenu::show_submenu(state, tray_menu.interaction.hover_index);
          } else {
            // 否则执行命令
            UI::TrayMenu::handle_menu_command(state, item.command_id);
          }
        }
      }
      break;
    }
  }
  return 0;
}

// 处理WM_KILLFOCUS消息 - 简单的焦点管理
auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& tray_menu = state.tray_menu;

  // 检查焦点是否转移到了菜单系统内的其他窗口
  HWND new_focus = GetFocus();
  if (new_focus != nullptr &&
      (new_focus == tray_menu.hwnd || new_focus == tray_menu.submenu_hwnd)) {
    Logger().debug("Focus transferred within menu system, no action needed，new_focus: {}",
                   (void*)new_focus);
    return 0;
  }

  Logger().debug("Menu lost focus to external window, hiding entire menu system");
  UI::TrayMenu::hide_menu(state);
  // UI::TrayMenu::hide_menu(state);
  return 0;
}

// 处理WM_TIMER消息（添加隐藏定时器支持）
auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT {
  auto& interaction = state.tray_menu.interaction;

  if (timer_id == interaction.SHOW_TIMER_ID && timer_id == interaction.show_timer_id) {
    Logger().debug("Show timer expired, showing submenu for index: {}",
                   interaction.pending_submenu_index);

    // 清理显示定时器
    KillTimer(hwnd, interaction.show_timer_id);
    interaction.show_timer_id = 0;

    // 验证待显示的索引仍然有效且鼠标仍在该菜单项上
    if (interaction.pending_submenu_index >= 0 &&
        interaction.pending_submenu_index < static_cast<int>(state.tray_menu.items.size()) &&
        interaction.pending_submenu_index == state.tray_menu.interaction.hover_index) {
      const auto& item = state.tray_menu.items[interaction.pending_submenu_index];
      if (item.has_submenu()) {
        UI::TrayMenu::show_submenu(state, interaction.pending_submenu_index);
      }
    }

    // 重置待显示索引
    interaction.pending_submenu_index = -1;
  } else if (timer_id == UI::TrayMenu::State::InteractionState::HIDE_TIMER_ID &&
             timer_id == interaction.hide_timer_id) {
    Logger().debug("Hide timer expired, hiding submenu for diagonal movement");

    // 清理隐藏定时器
    KillTimer(hwnd, interaction.hide_timer_id);
    interaction.hide_timer_id = 0;

    // 隐藏子菜单
    UI::TrayMenu::hide_submenu(state);
  }

  return 0;
}

}  // namespace UI::TrayMenu::MessageHandler