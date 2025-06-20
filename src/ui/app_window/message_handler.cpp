module;

#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.AppWindow.MessageHandler;

import std;
import Core.Actions;
import Core.Events;
import Core.State;
import Core.Constants;
import UI.AppWindow.Rendering;
import UI.TrayIcon;

namespace UI::AppWindow::MessageHandler {

// 内部函数声明
auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT;
auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void;
auto handle_mouse_leave(Core::State::AppState& state) -> void;
auto handle_left_click(Core::State::AppState& state, int x, int y) -> void;
auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void;
auto dispatch_item_click_event(Core::State::AppState& state, const Core::State::MenuItem& item)
    -> void;
auto handle_tray_command(Core::State::AppState& state, WORD command_id) -> void;
auto ensure_mouse_tracking(HWND hwnd) -> void;

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
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      if (HDC hdc = BeginPaint(hwnd, &ps); hdc) {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        paint_window(hdc, rect, state);
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
      if (pt.y < state.render.title_height) {
        return HTCAPTION;
      }
      return HTCLIENT;
    }

    case WM_CLOSE:
      Core::Events::post_event(state.event_bus,
                               {Core::Events::EventType::WindowAction,
                                Core::Events::WindowAction::Close, state.window.hwnd});
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 处理鼠标移动，更新悬停状态并重绘
auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void {
  const int new_hover_index = get_item_index_from_point(state, x, y);
  if (new_hover_index != state.ui.hover_index) {
    Core::Actions::dispatch_action(
        state, Core::Actions::Action{
                   Core::Actions::Payloads::UpdateHoverIndex{.index = new_hover_index}});

    Core::Actions::trigger_ui_update(state);
    ensure_mouse_tracking(state.window.hwnd);
  }
}

// 处理鼠标移出窗口，重置悬停状态并重绘
auto handle_mouse_leave(Core::State::AppState& state) -> void {
  Core::Actions::dispatch_action(
      state, Core::Actions::Action{Core::Actions::Payloads::UpdateHoverIndex{.index = -1}});

  Core::Actions::trigger_ui_update(state);
}

// 处理鼠标左键点击，分发项目点击事件
auto handle_left_click(Core::State::AppState& state, int x, int y) -> void {
  const int clicked_index = get_item_index_from_point(state, x, y);
  if (clicked_index >= 0 && clicked_index < static_cast<int>(state.data.menu_items.size())) {
    const auto& item = state.data.menu_items[clicked_index];
    dispatch_item_click_event(state, item);
  }
}

// 处理热键，发送系统命令事件
auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void {
  using namespace Core::Events;
  post_event(state.event_bus,
             {EventType::SystemCommand, std::string("toggle_visibility"), state.window.hwnd});
}

// 确保窗口能接收到WM_MOUSELEAVE消息
auto ensure_mouse_tracking(HWND hwnd) -> void {
  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd;
  TrackMouseEvent(&tme);
}

// 将菜单项点击转换为具体的高层应用事件
auto dispatch_item_click_event(Core::State::AppState& state, const Core::State::MenuItem& item)
    -> void {
  using namespace Core::Events;

  switch (item.type) {
    case Core::State::ItemType::Ratio: {
      const auto& ratio_preset = state.data.ratios[item.index];
      post_event(state.event_bus, {EventType::RatioChanged,
                                   RatioChangeData{static_cast<size_t>(item.index),
                                                   ratio_preset.name, ratio_preset.ratio},
                                   state.window.hwnd});
      break;
    }
    case Core::State::ItemType::Resolution: {
      const auto& res_preset = state.data.resolutions[item.index];
      post_event(state.event_bus,
                 {EventType::ResolutionChanged,
                  ResolutionChangeData{
                      static_cast<size_t>(item.index), res_preset.name,
                      res_preset.baseWidth * static_cast<uint64_t>(res_preset.baseHeight)},
                  state.window.hwnd});
      break;
    }
    case Core::State::ItemType::PreviewWindow:
      post_event(
          state.event_bus,
          {EventType::ToggleFeature,
           FeatureToggleData{FeatureType::Preview, !state.ui.preview_enabled}, state.window.hwnd});
      break;
    case Core::State::ItemType::OverlayWindow:
      post_event(
          state.event_bus,
          {EventType::ToggleFeature,
           FeatureToggleData{FeatureType::Overlay, !state.ui.overlay_enabled}, state.window.hwnd});
      break;
    case Core::State::ItemType::LetterboxWindow:
      post_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Letterbox, !state.ui.letterbox_enabled},
                  state.window.hwnd});
      break;
    case Core::State::ItemType::CaptureWindow:
      post_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Capture, state.window.hwnd});
      break;
    case Core::State::ItemType::OpenScreenshot:
      post_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Screenshot, state.window.hwnd});
      break;
    case Core::State::ItemType::Reset:
      post_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Reset, state.window.hwnd});
      break;
    case Core::State::ItemType::Close:
      post_event(state.event_bus,
                 {EventType::WindowAction, WindowAction::Close, state.window.hwnd});
      break;
    case Core::State::ItemType::Exit:
      post_event(state.event_bus, {EventType::WindowAction, WindowAction::Exit, state.window.hwnd});
      break;
  }
}

// 新增：处理来自托盘菜单的命令
auto handle_tray_command(Core::State::AppState& state, WORD command_id) -> void {
    using namespace Core::Events;

    if (command_id >= Core::Constants::ID_RATIO_BASE && command_id < Core::Constants::ID_RESOLUTION_BASE) {
        // Handle ratio selection
        const size_t index = command_id - Core::Constants::ID_RATIO_BASE;
        if (index < state.data.ratios.size()) {
            const auto& ratio_preset = state.data.ratios[index];
            post_event(state.event_bus, {EventType::RatioChanged,
                                         RatioChangeData{index, ratio_preset.name, ratio_preset.ratio},
                                         state.window.hwnd});
        }
        return;
    }

    if (command_id >= Core::Constants::ID_RESOLUTION_BASE && command_id < Core::Constants::ID_WINDOW_BASE) {
        // Handle resolution selection
        const size_t index = command_id - Core::Constants::ID_RESOLUTION_BASE;
        if (index < state.data.resolutions.size()) {
            const auto& res_preset = state.data.resolutions[index];
            post_event(state.event_bus,
                       {EventType::ResolutionChanged,
                        ResolutionChangeData{
                            index, res_preset.name,
                            res_preset.baseWidth * static_cast<uint64_t>(res_preset.baseHeight)},
                        state.window.hwnd});
        }
        return;
    }
    
    // Handle other commands
    switch (command_id) {
        case Core::Constants::ID_EXIT:
            post_event(state.event_bus, {EventType::WindowAction, WindowAction::Exit, state.window.hwnd});
            break;
        case Core::Constants::ID_RESET:
            post_event(state.event_bus, {EventType::WindowAction, WindowAction::Reset, state.window.hwnd});
            break;
        case Core::Constants::ID_CAPTURE_WINDOW:
            post_event(state.event_bus, {EventType::WindowAction, WindowAction::Capture, state.window.hwnd});
            break;
        case Core::Constants::ID_OPEN_SCREENSHOT:
             post_event(state.event_bus, {EventType::WindowAction, WindowAction::Screenshot, state.window.hwnd});
            break;
        case Core::Constants::ID_PREVIEW_WINDOW:
            post_event(state.event_bus, {EventType::ToggleFeature, FeatureToggleData{FeatureType::Preview, !state.ui.preview_enabled}, state.window.hwnd});
            break;
        case Core::Constants::ID_OVERLAY_WINDOW:
            post_event(state.event_bus, {EventType::ToggleFeature, FeatureToggleData{FeatureType::Overlay, !state.ui.overlay_enabled}, state.window.hwnd});
            break;
        case Core::Constants::ID_LETTERBOX_WINDOW:
            post_event(state.event_bus, {EventType::ToggleFeature, FeatureToggleData{FeatureType::Letterbox, !state.ui.letterbox_enabled}, state.window.hwnd});
            break;
        // Add other cases for config, hotkey, language, etc.
        // For example:
        // case Core::Constants::ID_CONFIG:
        //     post_event(state.event_bus, {EventType::SystemCommand, std::string("open_config"), state.window.hwnd});
        //     break;
    }
}

}  // namespace UI::AppWindow::MessageHandler