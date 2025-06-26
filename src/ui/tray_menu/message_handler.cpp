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

    case WM_MOUSELEAVE:
      return UI::TrayMenu::MessageHandler::handle_mouse_leave(state, hwnd);

    case WM_LBUTTONDOWN:
      return UI::TrayMenu::MessageHandler::handle_left_button_down(state, hwnd, wParam, lParam);

    case WM_KEYDOWN:
      return UI::TrayMenu::MessageHandler::handle_key_down(state, hwnd, wParam, lParam);

    case WM_KILLFOCUS:
      return UI::TrayMenu::MessageHandler::handle_kill_focus(state, hwnd);

    case WM_DESTROY: {
      state.tray_menu.hwnd = nullptr;
      state.tray_menu.is_created = false;
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
    UI::TrayMenu::Painter::paint_tray_menu(state, rect);
    EndPaint(hwnd, &ps);
  }
  return 0;
}

// 处理WM_SIZE消息
auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  // 窗口大小改变时，重新初始化D2D资源
  UI::TrayMenu::D2DContext::cleanup_d2d(state);

  // 获取新的窗口尺寸
  RECT rc;
  GetClientRect(hwnd, &rc);
  SIZE new_size = {rc.right - rc.left, rc.bottom - rc.top};

  // 重新初始化D2D资源
  UI::TrayMenu::D2DContext::initialize_d2d(state, hwnd);

  return 0;
}

// 处理WM_MOUSEMOVE消息
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  auto& tray_menu = state.tray_menu;
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  // 计算鼠标悬停的菜单项
  int hover_index = UI::TrayMenu::Layout::get_menu_item_at_point(state, pt);

  if (hover_index != tray_menu.interaction.hover_index) {
    tray_menu.interaction.hover_index = hover_index;
    InvalidateRect(hwnd, nullptr, FALSE);  // 触发重绘
  }

  // 开始鼠标跟踪
  if (!tray_menu.interaction.is_mouse_tracking) {
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);
    tray_menu.interaction.is_mouse_tracking = true;
  }

  return 0;
}

// 处理WM_MOUSELEAVE消息
auto handle_mouse_leave(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  auto& tray_menu = state.tray_menu;
  tray_menu.interaction.hover_index = -1;
  tray_menu.interaction.is_mouse_tracking = false;
  InvalidateRect(hwnd, nullptr, FALSE);  // 触发重绘
  return 0;
}

// 处理WM_LBUTTONDOWN消息
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  auto& tray_menu = state.tray_menu;
  if (tray_menu.interaction.hover_index >= 0 &&
      tray_menu.interaction.hover_index < static_cast<int>(tray_menu.items.size())) {
    const auto& item = tray_menu.items[tray_menu.interaction.hover_index];
    if (item.type == UI::TrayMenu::State::MenuItemType::Normal && item.is_enabled) {
      UI::TrayMenu::handle_menu_command(state, item.command_id);
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
    case VK_RETURN: {
      // 执行当前选中的菜单项
      if (tray_menu.interaction.hover_index >= 0 &&
          tray_menu.interaction.hover_index < static_cast<int>(tray_menu.items.size())) {
        const auto& item = tray_menu.items[tray_menu.interaction.hover_index];
        if (item.type == UI::TrayMenu::State::MenuItemType::Normal && item.is_enabled) {
          UI::TrayMenu::handle_menu_command(state, item.command_id);
        }
      }
      break;
    }
  }
  return 0;
}

// 处理WM_KILLFOCUS消息
auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  // 当菜单失去焦点时，立即隐藏菜单
  UI::TrayMenu::hide_menu(state);
  return 0;
}

}  // namespace UI::TrayMenu::MessageHandler