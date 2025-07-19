module;

#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.AppWindow.MessageHandler;

import std;
import Common.MenuData;
import Core.Events;
import Core.State;
import Core.Constants;
import UI.AppWindow;
import UI.AppWindow.Layout;
import UI.AppWindow.Painter;
import UI.AppWindow.State;
import UI.TrayIcon;
import UI.AppWindow.D2DContext;

namespace UI::AppWindow::MessageHandler {

// 确保窗口能接收到WM_MOUSELEAVE消息
auto ensure_mouse_tracking(HWND hwnd) -> void {
  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd;
  TrackMouseEvent(&tme);
}

// 将菜单项点击转换为具体的高层应用事件
auto dispatch_item_click_event(Core::State::AppState& state, const UI::AppWindow::MenuItem& item)
    -> void {
  using namespace Core::Events;

  switch (item.type) {
    case UI::AppWindow::ItemType::AspectRatio: {
      const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
      if (item.index >= 0 && static_cast<size_t>(item.index) < ratios.size()) {
        const auto& ratio_preset = ratios[item.index];
        send_event(state.event_bus, {EventType::RatioChanged,
                                     RatioChangeData{static_cast<size_t>(item.index),
                                                     ratio_preset.name, ratio_preset.ratio},
                                     state.app_window.window.hwnd});
      }
      break;
    }
    case UI::AppWindow::ItemType::Resolution: {
      const auto& resolutions = Common::MenuData::get_current_resolutions(state);
      if (item.index >= 0 && static_cast<size_t>(item.index) < resolutions.size()) {
        const auto& res_preset = resolutions[item.index];
        send_event(state.event_bus,
                   {EventType::ResolutionChanged,
                    ResolutionChangeData{
                        static_cast<size_t>(item.index), res_preset.name,
                        res_preset.baseWidth * static_cast<uint64_t>(res_preset.baseHeight)},
                    state.app_window.window.hwnd});
      }
      break;
    }
    case UI::AppWindow::ItemType::FeatureTogglePreview:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Preview, !state.app_window.ui.preview_enabled},
                  state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::FeatureToggleOverlay:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Overlay, !state.app_window.ui.overlay_enabled},
                  state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::FeatureToggleLetterbox:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Letterbox, !state.app_window.ui.letterbox_enabled},
                  state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::ScreenshotCapture:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Capture, state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::ScreenshotOpenFolder:
      send_event(state.event_bus, {EventType::WindowAction, WindowAction::Screenshots,
                                   state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::WindowResetTransform:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Reset, state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::PanelHide:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Hide, state.app_window.window.hwnd});
      break;
    case UI::AppWindow::ItemType::AppExit:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Exit, state.app_window.window.hwnd});
      break;
  }
}

// 处理来自托盘菜单的命令
auto handle_tray_command(Core::State::AppState& state, WORD command_id) -> void {
  using namespace Core::Events;

  if (command_id >= Core::Constants::ID_RATIO_BASE &&
      command_id < Core::Constants::ID_RESOLUTION_BASE) {
    // Handle ratio selection
    const size_t index = command_id - Core::Constants::ID_RATIO_BASE;
    const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
    if (index < ratios.size()) {
      const auto& ratio_preset = ratios[index];
      send_event(state.event_bus, {EventType::RatioChanged,
                                   RatioChangeData{index, ratio_preset.name, ratio_preset.ratio},
                                   state.app_window.window.hwnd});
    }
    return;
  }

  if (command_id >= Core::Constants::ID_RESOLUTION_BASE &&
      command_id < Core::Constants::ID_WINDOW_BASE) {
    // Handle resolution selection
    const size_t index = command_id - Core::Constants::ID_RESOLUTION_BASE;
    const auto& resolutions = Common::MenuData::get_current_resolutions(state);
    if (index < resolutions.size()) {
      const auto& res_preset = resolutions[index];
      send_event(state.event_bus,
                 {EventType::ResolutionChanged,
                  ResolutionChangeData{
                      index, res_preset.name,
                      res_preset.baseWidth * static_cast<uint64_t>(res_preset.baseHeight)},
                  state.app_window.window.hwnd});
    }
    return;
  }

  // Handle other commands
  switch (command_id) {
    case Core::Constants::ID_EXIT:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Exit, state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_RESET:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Reset, state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_CAPTURE_WINDOW:
      send_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Capture, state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_OPEN_SCREENSHOT:
      send_event(state.event_bus, {EventType::WindowAction, WindowAction::Screenshots,
                                   state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_PREVIEW_WINDOW:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Preview, !state.app_window.ui.preview_enabled},
                  state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_OVERLAY_WINDOW:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Overlay, !state.app_window.ui.overlay_enabled},
                  state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_LETTERBOX_WINDOW:
      send_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Letterbox, !state.app_window.ui.letterbox_enabled},
                  state.app_window.window.hwnd});
      break;
    case Core::Constants::ID_WEBVIEW_TEST:
      send_event(state.event_bus, {EventType::SystemCommand, std::string("webview_test"),
                                   state.app_window.window.hwnd});
      break;
      // Add other cases for config, hotkey, language, etc.
      // For example:
      // case Core::Constants::ID_CONFIG:
      //     send_event(state.event_bus, {EventType::SystemCommand, std::string("open_config"),
      //     state.app_window.window.hwnd}); break;
  }
}

// 处理热键，发送系统命令事件
auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void {
  using namespace Core::Events;
  send_event(state.event_bus, {EventType::SystemCommand, std::string("toggle_visibility"),
                               state.app_window.window.hwnd});
}

// 处理鼠标移出窗口，重置悬停状态并重绘
auto handle_mouse_leave(Core::State::AppState& state) -> void {
  // 重置悬停索引
  state.app_window.ui.hover_index = -1;

  UI::AppWindow::request_repaint(state);
}

// 处理鼠标移动，更新悬停状态并重绘
auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void {
  const int new_hover_index = get_item_index_from_point(state, x, y);
  if (new_hover_index != state.app_window.ui.hover_index) {
    // 更新悬停索引
    state.app_window.ui.hover_index = new_hover_index;

    UI::AppWindow::request_repaint(state);
    ensure_mouse_tracking(state.app_window.window.hwnd);
  }
}

// 处理鼠标左键点击，分发项目点击事件
auto handle_left_click(Core::State::AppState& state, int x, int y) -> void {
  const int clicked_index = get_item_index_from_point(state, x, y);
  if (clicked_index >= 0 &&
      clicked_index < static_cast<int>(state.app_window.data.menu_items.size())) {
    const auto& item = state.app_window.data.menu_items[clicked_index];
    dispatch_item_click_event(state, item);
  }
}

// 主窗口过程函数，负责将Windows消息翻译成应用程序事件
auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
  switch (msg) {
    case Core::Constants::WM_TRAYICON:
      if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
        UI::TrayIcon::show_context_menu(state);
      }
      return 0;

    case WM_COMMAND:
      handle_tray_command(state, LOWORD(wParam));
      return 0;

    case WM_HOTKEY:
      handle_hotkey(state, wParam);
      return 0;

    case WM_DPICHANGED: {
      const UINT dpi = HIWORD(wParam);
      Core::State::update_render_dpi(state, dpi);

      const auto window_size = calculate_window_size(state);
      RECT currentRect{};
      GetWindowRect(hwnd, &currentRect);

      SetWindowPos(hwnd, nullptr, currentRect.left, currentRect.top, window_size.cx, window_size.cy,
                   SWP_NOZORDER | SWP_NOACTIVATE);

      // 如果Direct2D已初始化，调整渲染目标大小
      if (state.d2d_render.is_initialized) {
        UI::AppWindow::D2DContext::resize_d2d(state, window_size);
      }

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
      if (pt.y < state.app_window.layout.title_height) {
        return HTCAPTION;
      }
      return HTCLIENT;
    }

    case WM_CLOSE:
      Core::Events::send_event(
          state.event_bus, {Core::Events::EventType::WindowAction, Core::Events::WindowAction::Hide,
                            state.app_window.window.hwnd});
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