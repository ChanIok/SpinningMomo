module;

#include <iostream>
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include <windows.h>
#include <windowsx.h>

module UI.AppWindow;

import std;
import Core.Constants;
import Core.Events;
import Core.State;
import UI.AppWindow.Rendering;

namespace UI::AppWindow {

auto create_window(Core::State::AppState& state)
    -> std::expected<void, std::string> {
  // 初始化菜单项
  initialize_menu_items(state, *state.data.strings);

  // 获取系统DPI
  UINT dpi = 96;
  if (HDC hdc = GetDC(nullptr); hdc) {
    dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
  }
  Core::State::update_render_dpi(state, dpi);

  // 计算窗口尺寸和位置
  const auto window_size = calculate_window_size(state);
  const auto window_pos = calculate_center_position(window_size);

  // 注册窗口类
  register_window_class(state.window.instance);

  // 创建窗口
  state.window.hwnd = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"SpinningMomoAppWindowClass",
      L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN, window_pos.x, window_pos.y, window_size.cx,
      window_size.cy, nullptr, nullptr, state.window.instance, &state);

  if (!state.window.hwnd) {
    return std::unexpected("Failed to create window");
  }

  // 保存窗口尺寸和位置
  state.window.size = window_size;
  state.window.position = window_pos;

  // 创建窗口属性
  create_window_attributes(state.window.hwnd);

  return {};
}

auto show_window(Core::State::AppState& state) -> void {
  if (state.window.hwnd) {
    ShowWindow(state.window.hwnd, SW_SHOWNA);
    UpdateWindow(state.window.hwnd);
    state.window.is_visible = true;
  }
}

auto hide_window(Core::State::AppState& state) -> void {
  if (state.window.hwnd) {
    ShowWindow(state.window.hwnd, SW_HIDE);
    state.window.is_visible = false;
  }
}

auto toggle_visibility(Core::State::AppState& state) -> void {
  if (is_window_visible(state)) {
    hide_window(state);
  } else {
    show_window(state);
  }
}

auto destroy_window(Core::State::AppState& state) -> void {
  unregister_hotkey(state);
  if (state.window.hwnd) {
    DestroyWindow(state.window.hwnd);
    state.window.hwnd = nullptr;
    state.window.is_visible = false;
  }
}

auto is_window_visible(const Core::State::AppState& state) -> bool {
  return state.window.hwnd && IsWindowVisible(state.window.hwnd);
}

auto activate_window(Core::State::AppState& state) -> void {
  if (state.window.hwnd) {
    SetForegroundWindow(state.window.hwnd);
  }
}

auto set_current_ratio(Core::State::AppState& state, size_t index) -> void {
  state.ui.current_ratio_index = index;
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto set_current_resolution(Core::State::AppState& state, size_t index) -> void {
  if (index < state.data.resolutions.size()) {
    state.ui.current_resolution_index = index;
    if (state.window.hwnd) {
      InvalidateRect(state.window.hwnd, nullptr, TRUE);
    }
  }
}

auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.ui.preview_enabled = enabled;
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.ui.overlay_enabled = enabled;
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.ui.letterbox_enabled = enabled;
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto update_menu_items(Core::State::AppState& state, const Core::Constants::LocalizedStrings& strings)
    -> void {
  state.data.menu_items.clear();
  initialize_menu_items(state, strings);
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto set_menu_items_to_show(Core::State::AppState& state, std::span<const std::wstring> items)
    -> void {
  state.data.menu_items_to_show.assign(items.begin(), items.end());
}

auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void {
  const int new_hover_index = get_item_index_from_point(state, x, y);
  if (new_hover_index != state.ui.hover_index) {
    state.ui.hover_index = new_hover_index;
    ensure_mouse_tracking(state.window.hwnd);
    if (state.window.hwnd) {
      InvalidateRect(state.window.hwnd, nullptr, TRUE);
    }
  }
}

auto handle_mouse_leave(Core::State::AppState& state) -> void {
  state.ui.hover_index = -1;
  if (state.window.hwnd) {
    InvalidateRect(state.window.hwnd, nullptr, TRUE);
  }
}

auto handle_left_click(Core::State::AppState& state, int x, int y) -> void {
  const int clicked_index = get_item_index_from_point(state, x, y);
  if (clicked_index >= 0 && clicked_index < static_cast<int>(state.data.menu_items.size())) {
    const auto& item = state.data.menu_items[clicked_index];
    dispatch_item_click_event(state, item);
  }
  hide_window(state);  // 点击后隐藏窗口
}

auto register_hotkey(Core::State::AppState& state, UINT modifiers, UINT key) -> bool {
  if (state.window.hwnd) {
    return ::RegisterHotKey(state.window.hwnd, state.window.hotkey_id, modifiers, key);
  }
  return false;
}

auto unregister_hotkey(Core::State::AppState& state) -> void {
  if (state.window.hwnd) {
    ::UnregisterHotKey(state.window.hwnd, state.window.hotkey_id);
  }
}

auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void {
  // 发送窗口可见性切换事件
  using namespace Core::Events;
  post_event(state.event_bus,
             {EventType::SystemCommand, std::string("toggle_visibility"), state.window.hwnd});
}

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

auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                      LPARAM lParam) -> LRESULT {
  switch (msg) {
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
      hide_window(state);
      return 0;

    case WM_DESTROY:
      destroy_window(state);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 内部辅助函数实现
auto register_window_class(HINSTANCE instance) -> void {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"SpinningMomoAppWindowClass";
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);
}

auto initialize_menu_items(Core::State::AppState& state, const Core::Constants::LocalizedStrings& strings)
    -> void {
  state.data.menu_items.clear();

  // 添加比例选项
  for (size_t i = 0; i < state.data.ratios.size(); ++i) {
    state.data.menu_items.push_back(
        {state.data.ratios[i].name, Core::State::ItemType::Ratio, static_cast<int>(i)});
  }

  // 添加分辨率选项
  for (size_t i = 0; i < state.data.resolutions.size(); ++i) {
    std::wstring displayText;
    const auto& preset = state.data.resolutions[i];
    if (preset.baseWidth == 0 && preset.baseHeight == 0) {
      displayText = preset.name;
    } else {
      const double megaPixels = preset.totalPixels / 1000000.0;
      wchar_t buffer[16];
      if (megaPixels < 10) {
        swprintf(buffer, 16, L"%.1f", megaPixels);
      } else {
        swprintf(buffer, 16, L"%.0f", megaPixels);
      }
      displayText = preset.name + L" (" + buffer + L"M)";
    }
    state.data.menu_items.push_back(
        {displayText, Core::State::ItemType::Resolution, static_cast<int>(i)});
  }

  // 添加设置选项
  if (!state.data.menu_items_to_show.empty()) {
    for (const auto& itemType : state.data.menu_items_to_show) {
      if (itemType == Core::Constants::MENU_ITEM_TYPE_CAPTURE) {
        state.data.menu_items.push_back(
            {strings.CAPTURE_WINDOW, Core::State::ItemType::CaptureWindow, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_SCREENSHOT) {
        state.data.menu_items.push_back(
            {strings.OPEN_SCREENSHOT, Core::State::ItemType::OpenScreenshot, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_PREVIEW) {
        state.data.menu_items.push_back(
            {strings.PREVIEW_WINDOW, Core::State::ItemType::PreviewWindow, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_OVERLAY) {
        state.data.menu_items.push_back(
            {strings.OVERLAY_WINDOW, Core::State::ItemType::OverlayWindow, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_LETTERBOX) {
        state.data.menu_items.push_back(
            {strings.LETTERBOX_WINDOW, Core::State::ItemType::LetterboxWindow, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_RESET) {
        state.data.menu_items.push_back({strings.RESET_WINDOW, Core::State::ItemType::Reset, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_CLOSE) {
        state.data.menu_items.push_back({strings.CLOSE_WINDOW, Core::State::ItemType::Close, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_TYPE_EXIT) {
        state.data.menu_items.push_back({strings.EXIT, Core::State::ItemType::Exit, 0});
      }
    }
  } else {
    // 默认菜单项
    state.data.menu_items.push_back(
        {strings.CAPTURE_WINDOW, Core::State::ItemType::CaptureWindow, 0});
    state.data.menu_items.push_back(
        {strings.OPEN_SCREENSHOT, Core::State::ItemType::OpenScreenshot, 0});
    state.data.menu_items.push_back(
        {strings.PREVIEW_WINDOW, Core::State::ItemType::PreviewWindow, 0});
    state.data.menu_items.push_back(
        {strings.OVERLAY_WINDOW, Core::State::ItemType::OverlayWindow, 0});
    state.data.menu_items.push_back(
        {strings.LETTERBOX_WINDOW, Core::State::ItemType::LetterboxWindow, 0});
    state.data.menu_items.push_back({strings.RESET_WINDOW, Core::State::ItemType::Reset, 0});
    state.data.menu_items.push_back({strings.CLOSE_WINDOW, Core::State::ItemType::Close, 0});
  }
}

auto ensure_mouse_tracking(HWND hwnd) -> void {
  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd;
  TrackMouseEvent(&tme);
}

auto create_window_attributes(HWND hwnd) -> void {
  // 设置窗口样式
  SetLayeredWindowAttributes(hwnd, 0, 204, LWA_ALPHA);

  // 设置DWM属性
  MARGINS margins{1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);

  DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
  DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));

  BOOL value = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));

  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

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
      post_event(
          state.event_bus,
          {EventType::ResolutionChanged,
           ResolutionChangeData{static_cast<size_t>(item.index), res_preset.name,
                                res_preset.baseWidth * static_cast<uint64_t>(res_preset.baseHeight)},
           state.window.hwnd});
      break;
    }
    case Core::State::ItemType::PreviewWindow:
      post_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Preview, !state.ui.preview_enabled},
                  state.window.hwnd});
      break;
    case Core::State::ItemType::OverlayWindow:
      post_event(state.event_bus,
                 {EventType::ToggleFeature,
                  FeatureToggleData{FeatureType::Overlay, !state.ui.overlay_enabled},
                  state.window.hwnd});
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
      post_event(state.event_bus, {EventType::WindowAction, WindowAction::Reset, state.window.hwnd});
      break;
    case Core::State::ItemType::Close:
      post_event(state.event_bus, {EventType::WindowAction, WindowAction::Close, state.window.hwnd});
      break;
    case Core::State::ItemType::Exit:
      post_event(state.event_bus, {EventType::WindowAction, WindowAction::Exit, state.window.hwnd});
      break;
  }
}

}  // namespace UI::AppWindow 