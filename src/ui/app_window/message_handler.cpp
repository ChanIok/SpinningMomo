module;

#include <windows.h>
#include <windowsx.h>

#include <string>

module UI.AppWindow.MessageHandler;

import std;
import Features.Settings.Menu;
import Core.Events;
import Core.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import UI.AppWindow.Layout;
import UI.AppWindow.Painter;
import UI.AppWindow.State;
import UI.AppWindow.Types;
import UI.TrayIcon;
import UI.TrayIcon.Types;
import UI.ContextMenu;
import UI.ContextMenu.Types;
import UI.AppWindow.D2DContext;
import Utils.Logger;

namespace UI::AppWindow::MessageHandler {

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
  const auto& render = state.app_window->layout;

  // 计算按钮尺寸（正方形，与标题栏高度一致）
  const int button_size = render.title_height;

  // 计算按钮位置（右上角）
  const int button_right = state.app_window->window.size.cx;
  const int button_left = button_right - button_size;
  const int button_top = 0;
  const int button_bottom = button_size;

  return (x >= button_left && x <= button_right && y >= button_top && y <= button_bottom);
}

// 基于 action_id 分发功能事件
auto dispatch_feature_action(Core::State::AppState& state, const std::string& action_id) -> void {
  using namespace UI::AppWindow::Events;

  // 直接使用字符串比较分发事件
  if (action_id == "feature.toggle_preview") {
    Core::Events::send(*state.events,
                       PreviewToggleEvent{!state.app_window->ui.preview_enabled});
  } else if (action_id == "feature.toggle_overlay") {
    Core::Events::send(*state.events,
                       OverlayToggleEvent{!state.app_window->ui.overlay_enabled});
  } else if (action_id == "feature.toggle_letterbox") {
    Core::Events::send(*state.events,
                       LetterboxToggleEvent{!state.app_window->ui.letterbox_enabled});
  } else if (action_id == "feature.toggle_recording") {
    Core::Events::send(*state.events,
                       RecordingToggleEvent{!state.app_window->ui.recording_enabled});
  } else if (action_id == "screenshot.capture") {
    Core::Events::send(*state.events, CaptureEvent{});
  } else if (action_id == "screenshot.open_folder") {
    Core::Events::send(*state.events, ScreenshotsEvent{});
  } else if (action_id == "window.reset_transform") {
    Core::Events::send(*state.events, ResetEvent{});
  } else if (action_id == "panel.hide") {
    Core::Events::send(*state.events, ToggleVisibilityEvent{});
  } else if (action_id == "app.exit") {
    Core::Events::send(*state.events, ExitEvent{});
  }
  // 未知的菜单ID，忽略
}

// 将菜单项点击转换为具体的高层应用事件
auto dispatch_item_click_event(Core::State::AppState& state, const UI::AppWindow::MenuItem& item)
    -> void {
  using namespace UI::AppWindow::Events;

  switch (item.category) {
    case UI::AppWindow::MenuItemCategory::AspectRatio: {
      const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
      if (item.index >= 0 && static_cast<size_t>(item.index) < ratios.size()) {
        const auto& ratio_preset = ratios[item.index];
        Core::Events::send(*state.events,
                           RatioChangeEvent{static_cast<size_t>(item.index), ratio_preset.name,
                                            ratio_preset.ratio});
      }
      break;
    }
    case UI::AppWindow::MenuItemCategory::Resolution: {
      const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
      if (item.index >= 0 && static_cast<size_t>(item.index) < resolutions.size()) {
        const auto& res_preset = resolutions[item.index];
        Core::Events::send(*state.events,
                           ResolutionChangeEvent{static_cast<size_t>(item.index), res_preset.name,
                                                 res_preset.base_width *
                                                     static_cast<uint64_t>(res_preset.base_height)});
      }
      break;
    }
    case UI::AppWindow::MenuItemCategory::Feature: {
      dispatch_feature_action(state, item.action_id);
      break;
    }
  }
}

// 处理热键，发送系统命令事件
auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void {
  using namespace UI::AppWindow::Events;
  
  // 根据热键ID分发不同事件
  if (hotkey_id == state.app_window->window.toggle_visibility_hotkey_id) {
    Core::Events::send(*state.events, ToggleVisibilityEvent{});
  } else if (hotkey_id == state.app_window->window.screenshot_hotkey_id) {
    Core::Events::send(*state.events, CaptureEvent{});
  }
}

// 处理鼠标移出窗口，重置悬停状态并重绘
auto handle_mouse_leave(Core::State::AppState& state) -> void {
  // 重置悬停索引
  state.app_window->ui.hover_index = -1;

  // 重置关闭按钮悬停状态
  state.app_window->ui.close_button_hovered = false;

  UI::AppWindow::request_repaint(state);
}

// 处理鼠标移动，更新悬停状态并重绘
auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void {
  const int new_hover_index = UI::AppWindow::Layout::get_item_index_from_point(state, x, y);
  if (new_hover_index != state.app_window->ui.hover_index) {
    // 更新悬停索引
    state.app_window->ui.hover_index = new_hover_index;

    UI::AppWindow::request_repaint(state);
    ensure_mouse_tracking(state.app_window->window.hwnd);
  }

  // 检查关闭按钮悬停状态
  const bool close_hovered = is_mouse_on_close_button(state, x, y);
  if (close_hovered != state.app_window->ui.close_button_hovered) {
    state.app_window->ui.close_button_hovered = close_hovered;
    UI::AppWindow::request_repaint(state);
    ensure_mouse_tracking(state.app_window->window.hwnd);
  }
}

// 处理鼠标左键点击，分发项目点击事件
auto handle_left_click(Core::State::AppState& state, int x, int y) -> void {
  // 检查是否点击了关闭按钮
  if (is_mouse_on_close_button(state, x, y)) {
    // 发送隐藏事件而不是退出事件
    Core::Events::send(*state.events, UI::AppWindow::Events::HideEvent{});
    return;
  }

  const int clicked_index = UI::AppWindow::Layout::get_item_index_from_point(state, x, y);
  if (clicked_index >= 0 &&
      clicked_index < static_cast<int>(state.app_window->data.menu_items.size())) {
    const auto& item = state.app_window->data.menu_items[clicked_index];
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
      const auto window_size = UI::AppWindow::Layout::calculate_window_size(state);

      // 发送DPI改变事件来更新渲染状态
      Core::Events::send(*state.events, UI::AppWindow::Events::DpiChangeEvent{dpi, window_size});

      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      if (HDC hdc = BeginPaint(hwnd, &ps); hdc) {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        UI::AppWindow::Painter::paint_app_window(state, hwnd, rect);
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

    case WM_NCHITTEST: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hwnd, &pt);

      // 检查是否在关闭按钮上
      if (is_mouse_on_close_button(state, pt.x, pt.y)) {
        return HTCLIENT;  // 关闭按钮区域不支持拖动
      }

      if (pt.y < state.app_window->layout.title_height) {
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
      UI::AppWindow::D2DContext::resize_d2d(state, new_size);
      return 0;
    }

    case WM_CLOSE:
      Core::Events::send(*state.events, UI::AppWindow::Events::HideEvent{});
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

}  // namespace UI::AppWindow::MessageHandler