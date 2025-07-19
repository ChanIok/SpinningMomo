module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.AppWindow;

import std;
import Common.MenuData;
import Core.Constants;
import Core.Events;
import Core.State;
import UI.AppWindow.MessageHandler;
import UI.AppWindow.Layout;
import UI.AppWindow.D2DContext;
import UI.AppWindow.Painter;
import Utils.Logger;

namespace UI::AppWindow {

auto create_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 初始化菜单项
  initialize_menu_items(state, *state.app_window.data.strings);

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

  register_window_class(state.app_window.window.instance);

  state.app_window.window.hwnd = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"SpinningMomoAppWindowClass",
      L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN, window_pos.x, window_pos.y, window_size.cx,
      window_size.cy, nullptr, nullptr, state.app_window.window.instance, &state);

  if (!state.app_window.window.hwnd) {
    return std::unexpected("Failed to create window");
  }

  // 保存窗口尺寸和位置
  state.app_window.window.size = window_size;
  state.app_window.window.position = window_pos;

  // 创建窗口属性
  create_window_attributes(state.app_window.window.hwnd);

  // 初始化Direct2D渲染
  if (!UI::AppWindow::D2DContext::initialize_d2d(state, state.app_window.window.hwnd)) {
    // Direct2D初始化失败，但不影响窗口创建，会回退到GDI渲染
    Logger().warn("Failed to initialize Direct2D rendering");
  }

  return {};
}

// 标准化渲染触发机制
auto request_repaint(Core::State::AppState& state) -> void {
  if (state.app_window.window.hwnd && state.app_window.window.is_visible) {
    InvalidateRect(state.app_window.window.hwnd, nullptr, FALSE);
  }
}

auto show_window(Core::State::AppState& state) -> void {
  if (state.app_window.window.hwnd) {
    ShowWindow(state.app_window.window.hwnd, SW_SHOWNA);
    UpdateWindow(state.app_window.window.hwnd);
    state.app_window.window.is_visible = true;

    // 触发初始绘制（使用标准InvalidateRect机制）
    request_repaint(state);
  }
}

auto hide_window(Core::State::AppState& state) -> void {
  if (state.app_window.window.hwnd) {
    ShowWindow(state.app_window.window.hwnd, SW_HIDE);
    state.app_window.window.is_visible = false;
  }
}

auto toggle_visibility(Core::State::AppState& state) -> void {
  if (state.app_window.window.is_visible) {
    hide_window(state);
  } else {
    show_window(state);
  }
}

auto destroy_window(Core::State::AppState& state) -> void {
  unregister_hotkey(state);

  // 清理Direct2D资源
  UI::AppWindow::D2DContext::cleanup_d2d(state);

  if (state.app_window.window.hwnd) {
    DestroyWindow(state.app_window.window.hwnd);
    state.app_window.window.hwnd = nullptr;
    state.app_window.window.is_visible = false;
  }
}

auto set_current_ratio(Core::State::AppState& state, size_t index) -> void {
  state.app_window.ui.current_ratio_index = index;
  if (state.app_window.window.hwnd) {
    request_repaint(state);
  }
}

auto set_current_resolution(Core::State::AppState& state, size_t index) -> void {
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  if (index < resolutions.size()) {
    state.app_window.ui.current_resolution_index = index;
    if (state.app_window.window.hwnd) {
      request_repaint(state);
    }
  }
}

auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window.ui.preview_enabled = enabled;
  if (state.app_window.window.hwnd) {
    request_repaint(state);
  }
}

auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window.ui.overlay_enabled = enabled;
  if (state.app_window.window.hwnd) {
    request_repaint(state);
  }
}

auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window.ui.letterbox_enabled = enabled;
  if (state.app_window.window.hwnd) {
    request_repaint(state);
  }
}

auto update_menu_items(Core::State::AppState& state,
                       const Core::Constants::LocalizedStrings& strings) -> void {
  state.app_window.data.menu_items.clear();
  initialize_menu_items(state, strings);
  if (state.app_window.window.hwnd) {
    request_repaint(state);
  }
}

auto set_menu_items_to_show(Core::State::AppState& state, std::span<const std::wstring> items)
    -> void {
  state.app_window.data.menu_items_to_show.assign(items.begin(), items.end());
}

auto register_hotkey(Core::State::AppState& state, UINT modifiers, UINT key) -> bool {
  if (state.app_window.window.hwnd) {
    return ::RegisterHotKey(state.app_window.window.hwnd, state.app_window.window.hotkey_id,
                            modifiers, key);
  }
  return false;
}

auto unregister_hotkey(Core::State::AppState& state) -> void {
  if (state.app_window.window.hwnd) {
    ::UnregisterHotKey(state.app_window.window.hwnd, state.app_window.window.hotkey_id);
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
  state.app_window.data.menu_items.clear();

  // 从settings模块获取数据，而不是从app_window.data
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);

  // 添加比例选项
  for (size_t i = 0; i < ratios.size(); ++i) {
    state.app_window.data.menu_items.push_back({ratios[i].name,
                                                UI::AppWindow::ItemType::AspectRatio,
                                                static_cast<int>(i)});
  }

  // 添加分辨率选项
  for (size_t i = 0; i < resolutions.size(); ++i) {
    std::wstring displayText;
    const auto& preset = resolutions[i];
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
    state.app_window.data.menu_items.push_back(
        {displayText, UI::AppWindow::ItemType::Resolution, static_cast<int>(i)});
  }

  // 添加设置选项
  if (!state.app_window.data.menu_items_to_show.empty()) {
    for (const auto& itemType : state.app_window.data.menu_items_to_show) {
      if (itemType == Core::Constants::MENU_ITEM_ID_SCREENSHOT_CAPTURE) {
        state.app_window.data.menu_items.push_back(
            {strings.CAPTURE_WINDOW, UI::AppWindow::ItemType::ScreenshotCapture, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_SCREENSHOT_OPEN_FOLDER) {
        state.app_window.data.menu_items.push_back(
            {strings.OPEN_SCREENSHOT, UI::AppWindow::ItemType::ScreenshotOpenFolder, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_FEATURE_TOGGLE_PREVIEW) {
        state.app_window.data.menu_items.push_back(
            {strings.PREVIEW_WINDOW, UI::AppWindow::ItemType::FeatureTogglePreview, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_FEATURE_TOGGLE_OVERLAY) {
        state.app_window.data.menu_items.push_back(
            {strings.OVERLAY_WINDOW, UI::AppWindow::ItemType::FeatureToggleOverlay, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_FEATURE_TOGGLE_LETTERBOX) {
        state.app_window.data.menu_items.push_back(
            {strings.LETTERBOX_WINDOW, UI::AppWindow::ItemType::FeatureToggleLetterbox, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_WINDOW_RESET_TRANSFORM) {
        state.app_window.data.menu_items.push_back(
            {strings.RESET_WINDOW, UI::AppWindow::ItemType::WindowResetTransform, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_PANEL_HIDE) {
        state.app_window.data.menu_items.push_back(
            {strings.CLOSE_WINDOW, UI::AppWindow::ItemType::PanelHide, 0});
      } else if (itemType == Core::Constants::MENU_ITEM_ID_APP_EXIT) {
        state.app_window.data.menu_items.push_back(
            {strings.EXIT, UI::AppWindow::ItemType::AppExit, 0});
      }
    }
  } else {
    // 默认菜单项
    state.app_window.data.menu_items.push_back(
        {strings.CAPTURE_WINDOW, UI::AppWindow::ItemType::ScreenshotCapture, 0});
    state.app_window.data.menu_items.push_back(
        {strings.OPEN_SCREENSHOT, UI::AppWindow::ItemType::ScreenshotOpenFolder, 0});
    state.app_window.data.menu_items.push_back(
        {strings.PREVIEW_WINDOW, UI::AppWindow::ItemType::FeatureTogglePreview, 0});
    state.app_window.data.menu_items.push_back(
        {strings.OVERLAY_WINDOW, UI::AppWindow::ItemType::FeatureToggleOverlay, 0});
    state.app_window.data.menu_items.push_back(
        {strings.LETTERBOX_WINDOW, UI::AppWindow::ItemType::FeatureToggleLetterbox, 0});
    state.app_window.data.menu_items.push_back(
        {strings.RESET_WINDOW, UI::AppWindow::ItemType::WindowResetTransform, 0});
    state.app_window.data.menu_items.push_back(
        {strings.CLOSE_WINDOW, UI::AppWindow::ItemType::PanelHide, 0});
    state.app_window.data.menu_items.push_back(
        {strings.EXIT, UI::AppWindow::ItemType::AppExit, 0});
  }
}

auto create_window_attributes(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// 设置变更响应实现
auto refresh_from_settings(Core::State::AppState& state) -> void {
  if (state.app_window.data.strings) {
    update_menu_items(state, *state.app_window.data.strings);
    request_repaint(state);
  }
}

}  // namespace UI::AppWindow