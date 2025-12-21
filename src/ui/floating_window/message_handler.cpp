module;

#include <windows.h>
#include <windowsx.h>

#include <string>

module UI.FloatingWindow.MessageHandler;

import std;
import Features.Settings.Menu;
import Core.Commands;
import Core.Commands.State;
import Core.Events;
import Core.State;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import UI.FloatingWindow.Layout;
import UI.FloatingWindow.Painter;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Types;
import UI.TrayIcon;
import UI.TrayIcon.Types;
import UI.ContextMenu;
import UI.ContextMenu.Types;
import UI.FloatingWindow.D2DContext;
import Utils.Logger;

namespace UI::FloatingWindow::MessageHandler {

// 统计指定列的项目数量
auto count_column_items(const std::vector<UI::FloatingWindow::MenuItem>& items,
                        UI::FloatingWindow::MenuItemCategory category) -> size_t {
  size_t count = 0;
  for (const auto& item : items) {
    if (item.category == category) {
      count++;
    }
  }
  return count;
}

// 确保窗口能接收到WM_MOUSELEAVE消息
auto ensure_mouse_tracking(HWND hwnd) -> void {
  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd;
  TrackMouseEvent(&tme);
}

// 检查鼠标是否在关闭按钮上
auto is_mouse_on_close_button(const Core::State::AppState& state, int x, int y) -> bool {
  const auto& render = state.floating_window->layout;

  // 计算按钮尺寸（正方形，与标题栏高度一致）
  const int button_size = render.title_height;

  // 计算按钮位置（右上角）
  const int button_right = state.floating_window->window.size.cx;
  const int button_left = button_right - button_size;
  const int button_top = 0;
  const int button_bottom = button_size;

  return (x >= button_left && x <= button_right && y >= button_top && y <= button_bottom);
}

// 将菜单项点击转换为具体的高层应用事件
auto dispatch_item_click_event(Core::State::AppState& state,
                               const UI::FloatingWindow::MenuItem& item) -> void {
  using namespace UI::FloatingWindow::Events;

  switch (item.category) {
    case UI::FloatingWindow::MenuItemCategory::AspectRatio: {
      const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
      if (item.index >= 0 && static_cast<size_t>(item.index) < ratios.size()) {
        const auto& ratio_preset = ratios[item.index];
        Core::Events::send(*state.events, RatioChangeEvent{static_cast<size_t>(item.index),
                                                           ratio_preset.name, ratio_preset.ratio});
      }
      break;
    }
    case UI::FloatingWindow::MenuItemCategory::Resolution: {
      const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
      if (item.index >= 0 && static_cast<size_t>(item.index) < resolutions.size()) {
        const auto& res_preset = resolutions[item.index];
        Core::Events::send(*state.events, ResolutionChangeEvent{
                                              static_cast<size_t>(item.index), res_preset.name,
                                              res_preset.base_width *
                                                  static_cast<uint64_t>(res_preset.base_height)});
      }
      break;
    }
    case UI::FloatingWindow::MenuItemCategory::Feature: {
      // 通过注册表调用命令
      if (state.commands) {
        Core::Commands::invoke_command(state.commands->registry, item.action_id);
      }
      break;
    }
  }
}

// 处理热键，通过命令注册表调用
auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void {
  // 根据热键ID调用对应命令
  if (hotkey_id == state.floating_window->window.toggle_visibility_hotkey_id) {
    if (state.commands) {
      Core::Commands::invoke_command(state.commands->registry, "app.float");
    }
  } else if (hotkey_id == state.floating_window->window.screenshot_hotkey_id) {
    if (state.commands) {
      Core::Commands::invoke_command(state.commands->registry, "screenshot.capture");
    }
  }
}

// 处理鼠标移出窗口，重置悬停状态并重绘
auto handle_mouse_leave(Core::State::AppState& state) -> void {
  // 重置悬停索引
  state.floating_window->ui.hover_index = -1;

  // 重置关闭按钮悬停状态
  state.floating_window->ui.close_button_hovered = false;

  // 重置 hovered_column
  state.floating_window->ui.hovered_column = -1;

  UI::FloatingWindow::request_repaint(state);
}

// 处理鼠标移动，更新悬停状态并重绘
auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void {
  const int new_hover_index = UI::FloatingWindow::Layout::get_item_index_from_point(state, x, y);
  if (new_hover_index != state.floating_window->ui.hover_index) {
    // 更新悬停索引
    state.floating_window->ui.hover_index = new_hover_index;

    UI::FloatingWindow::request_repaint(state);
    ensure_mouse_tracking(state.floating_window->window.hwnd);
  }

  // 检查关闭按钮悬停状态
  const bool close_hovered = is_mouse_on_close_button(state, x, y);
  if (close_hovered != state.floating_window->ui.close_button_hovered) {
    state.floating_window->ui.close_button_hovered = close_hovered;
    UI::FloatingWindow::request_repaint(state);
    ensure_mouse_tracking(state.floating_window->window.hwnd);
  }

  // 更新 hovered_column 状态
  const auto& render = state.floating_window->layout;
  const auto bounds = UI::FloatingWindow::Layout::get_column_bounds(state);

  int new_hovered_column = -1;
  if (y >= render.title_height + render.separator_height) {
    if (x < bounds.ratio_column_right) {
      new_hovered_column = 0;  // 比例列
    } else if (x >= bounds.ratio_column_right + render.separator_height &&
               x < bounds.resolution_column_right) {
      new_hovered_column = 1;  // 分辨率列
    } else if (x >= bounds.resolution_column_right + render.separator_height) {
      new_hovered_column = 2;  // 功能列
    }
  }

  if (new_hovered_column != state.floating_window->ui.hovered_column) {
    state.floating_window->ui.hovered_column = new_hovered_column;
    UI::FloatingWindow::request_repaint(state);
  }
}

// 处理鼠标左键点击，分发项目点击事件
auto handle_left_click(Core::State::AppState& state, int x, int y) -> void {
  // 检查是否点击了关闭按钮
  if (is_mouse_on_close_button(state, x, y)) {
    // 发送隐藏事件而不是退出事件
    Core::Events::send(*state.events, UI::FloatingWindow::Events::HideEvent{});
    return;
  }

  const int clicked_index = UI::FloatingWindow::Layout::get_item_index_from_point(state, x, y);
  if (clicked_index >= 0 &&
      clicked_index < static_cast<int>(state.floating_window->data.menu_items.size())) {
    const auto& item = state.floating_window->data.menu_items[clicked_index];
    dispatch_item_click_event(state, item);
  }
}

// 主窗口过程函数，负责将Windows消息翻译成应用程序事件
auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
  switch (msg) {
    case UI::TrayIcon::Types::WM_TRAYICON:
      if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
        UI::TrayIcon::show_context_menu(state);
      }
      return 0;

    case WM_HOTKEY:
      handle_hotkey(state, wParam);
      return 0;

    case WM_DPICHANGED: {
      const UINT dpi = HIWORD(wParam);
      const auto window_size = UI::FloatingWindow::Layout::calculate_window_size(state);

      // 发送DPI改变事件来更新渲染状态
      Core::Events::send(*state.events,
                         UI::FloatingWindow::Events::DpiChangeEvent{dpi, window_size});

      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      if (HDC hdc = BeginPaint(hwnd, &ps); hdc) {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        UI::FloatingWindow::Painter::paint_app_window(state, hwnd, rect);
        EndPaint(hwnd, &ps);
      }
      return 0;
    }

    case WM_MOUSEMOVE: {
      handle_mouse_move(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      return 0;
    }

    case WM_MOUSELEAVE: {
      handle_mouse_leave(state);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      handle_left_click(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      return 0;
    }

    case WM_MOUSEWHEEL: {
      // 只在翻页模式下处理滚轮
      if (state.floating_window->layout.layout_mode != UI::FloatingWindow::MenuLayoutMode::Paged) {
        return 0;
      }

      // 将屏幕坐标转换为客户端坐标
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hwnd, &pt);

      // 判断鼠标在哪一列（排除分隔线区域）
      const auto& render = state.floating_window->layout;
      const auto bounds = UI::FloatingWindow::Layout::get_column_bounds(state);
      const auto& items = state.floating_window->data.menu_items;
      auto& ui = state.floating_window->ui;

      size_t* target_offset = nullptr;
      size_t column_item_count = 0;

      if (pt.x < bounds.ratio_column_right) {
        // 比例列
        target_offset = &ui.ratio_scroll_offset;
        column_item_count =
            count_column_items(items, UI::FloatingWindow::MenuItemCategory::AspectRatio);
      } else if (pt.x >= bounds.ratio_column_right + render.separator_height &&
                 pt.x < bounds.resolution_column_right) {
        // 分辨率列（排除第一条分隔线）
        target_offset = &ui.resolution_scroll_offset;
        column_item_count =
            count_column_items(items, UI::FloatingWindow::MenuItemCategory::Resolution);
      } else if (pt.x >= bounds.resolution_column_right + render.separator_height) {
        // 功能列（排除第二条分隔线）
        target_offset = &ui.feature_scroll_offset;
        column_item_count =
            count_column_items(items, UI::FloatingWindow::MenuItemCategory::Feature);
      } else {
        // 在分隔线上，不处理
        return 0;
      }

      // 计算当前页号
      const int page_size = static_cast<int>(UI::FloatingWindow::LayoutConfig::MAX_VISIBLE_ROWS);
      const int current_page = static_cast<int>(*target_offset) / page_size;

      // 滚轮方向：向上滚-1页，向下滚+1页
      const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      const int page_delta = (delta > 0) ? -1 : 1;
      const int new_page = current_page + page_delta;

      // 计算总页数
      const int total_pages = (static_cast<int>(column_item_count) + page_size - 1) / page_size;

      // 限制页号范围并计算新的offset（必须是页大小的整数倍）
      const int clamped_page = std::clamp(new_page, 0, std::max(0, total_pages - 1));
      *target_offset = static_cast<size_t>(clamped_page * page_size);

      UI::FloatingWindow::request_repaint(state);
      return 0;
    }

    case WM_NCHITTEST: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hwnd, &pt);

      // 检查是否在关闭按钮上
      if (is_mouse_on_close_button(state, pt.x, pt.y)) {
        return HTCLIENT;  // 关闭按钮区域不支持拖动
      }

      if (pt.y < state.floating_window->layout.title_height) {
        return HTCAPTION;
      }
      return HTCLIENT;
    }

    case WM_RBUTTONUP: {
      // 复用托盘菜单的逻辑来显示上下文菜单
      UI::TrayIcon::show_context_menu(state);
      return 0;
    }

    case WM_SIZE: {
      SIZE new_size = {LOWORD(lParam), HIWORD(lParam)};
      // 调整Direct2D渲染上下文以适应新的窗口大小
      UI::FloatingWindow::D2DContext::resize_d2d(state, new_size);
      return 0;
    }

    case WM_CLOSE:
      Core::Events::send(*state.events, UI::FloatingWindow::Events::HideEvent{});
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 静态窗口过程函数，将窗口句柄与应用程序状态关联起来
LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  Core::State::AppState* state = nullptr;

  if (msg == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (state) {
    return window_procedure(*state, hwnd, msg, wParam, lParam);
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

}  // namespace UI::FloatingWindow::MessageHandler