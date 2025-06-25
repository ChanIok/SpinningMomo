module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.AppWindow;

import std;
import Core.Constants;
import Core.Events;
import Core.State;
import UI.AppWindow.MessageHandler;
import UI.AppWindow.Rendering;
import UI.Rendering.D2DInit;
import Utils.Logger;

namespace UI::AppWindow {

auto create_window(Core::State::AppState& state) -> std::expected<void, std::string> {
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

  register_window_class(state.window.instance);

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

  // 初始化Direct2D渲染
  if (auto result = UI::Rendering::D2DInit::initialize_d2d(state, state.window.hwnd); !result) {
    // Direct2D初始化失败，但不影响窗口创建，会回退到GDI渲染
    Logger().warn("Failed to initialize Direct2D rendering: {}", result.error());
  }

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

  // 清理Direct2D资源
  UI::Rendering::D2DInit::cleanup_d2d(state);

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

auto update_menu_items(Core::State::AppState& state,
                       const Core::Constants::LocalizedStrings& strings) -> void {
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

// 内部辅助函数实现
auto register_window_class(HINSTANCE instance) -> void {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = MessageHandler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"SpinningMomoAppWindowClass";
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);
}

auto initialize_menu_items(Core::State::AppState& state,
                           const Core::Constants::LocalizedStrings& strings) -> void {
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
        state.data.menu_items.push_back({strings.CLOSE_WINDOW, Core::State::ItemType::Hide, 0});
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
    state.data.menu_items.push_back({strings.CLOSE_WINDOW, Core::State::ItemType::Hide, 0});
  }
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

}  // namespace UI::AppWindow