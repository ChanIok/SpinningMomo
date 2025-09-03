module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <string>

module UI.AppWindow;

import std;
import Common.MenuData;
import Common.MenuData.Types;
import Core.Events;
import Core.State;
import UI.AppWindow.Events;
import UI.AppWindow.MessageHandler;
import UI.AppWindow.Layout;
import UI.AppWindow.D2DContext;
import UI.AppWindow.Painter;
import UI.AppWindow.State;
import Utils.Logger;

namespace UI::AppWindow {

auto create_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 获取系统DPI
  UINT dpi = 96;
  if (HDC hdc = GetDC(nullptr); hdc) {
    dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
  }

  // 保存DPI到状态中
  state.app_window->window.dpi = dpi;

  // 应用布局配置（基于应用状态）
  UI::AppWindow::Layout::update_layout(state);

  // 初始化菜单项
  initialize_menu_items(state);

  // 计算窗口尺寸和位置
  const auto window_size = UI::AppWindow::Layout::calculate_window_size(state);
  const auto window_pos = UI::AppWindow::Layout::calculate_center_position(window_size);

  // 发送DPI改变事件来更新渲染状态
  Core::Events::send(*state.events, UI::AppWindow::Events::DpiChangeEvent{dpi, window_size});

  register_window_class(state.app_window->window.instance);

  state.app_window->window.hwnd = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"SpinningMomoAppWindowClass",
      L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN, window_pos.x, window_pos.y, window_size.cx,
      window_size.cy, nullptr, nullptr, state.app_window->window.instance, &state);

  if (!state.app_window->window.hwnd) {
    return std::unexpected("Failed to create window");
  }

  // 保存窗口尺寸和位置
  state.app_window->window.size = window_size;
  state.app_window->window.position = window_pos;

  // 创建窗口属性
  create_window_attributes(state.app_window->window.hwnd);

  // 初始化Direct2D渲染
  if (!UI::AppWindow::D2DContext::initialize_d2d(state, state.app_window->window.hwnd)) {
    Logger().error("Failed to initialize Direct2D rendering");
  }

  return {};
}

// 标准化渲染触发机制
auto request_repaint(Core::State::AppState& state) -> void {
  if (state.app_window->window.hwnd && state.app_window->window.is_visible) {
    InvalidateRect(state.app_window->window.hwnd, nullptr, FALSE);
  }
}

auto show_window(Core::State::AppState& state) -> void {
  if (state.app_window->window.hwnd) {
    ShowWindow(state.app_window->window.hwnd, SW_SHOWNA);
    UpdateWindow(state.app_window->window.hwnd);
    state.app_window->window.is_visible = true;

    // 触发初始绘制（使用标准InvalidateRect机制）
    request_repaint(state);
  }
}

auto hide_window(Core::State::AppState& state) -> void {
  if (state.app_window->window.hwnd) {
    ShowWindow(state.app_window->window.hwnd, SW_HIDE);
    state.app_window->window.is_visible = false;
  }
}

auto toggle_visibility(Core::State::AppState& state) -> void {
  if (state.app_window->window.is_visible) {
    hide_window(state);
  } else {
    show_window(state);
  }
}

auto destroy_window(Core::State::AppState& state) -> void {
  unregister_hotkey(state);

  // 清理Direct2D资源
  UI::AppWindow::D2DContext::cleanup_d2d(state);

  if (state.app_window->window.hwnd) {
    DestroyWindow(state.app_window->window.hwnd);
    state.app_window->window.hwnd = nullptr;
    state.app_window->window.is_visible = false;
  }
}

auto set_current_ratio(Core::State::AppState& state, size_t index) -> void {
  state.app_window->ui.current_ratio_index = index;
  if (state.app_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_current_resolution(Core::State::AppState& state, size_t index) -> void {
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  if (index < resolutions.size()) {
    state.app_window->ui.current_resolution_index = index;
    if (state.app_window->window.hwnd) {
      request_repaint(state);
    }
  }
}

auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window->ui.preview_enabled = enabled;
  if (state.app_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window->ui.overlay_enabled = enabled;
  if (state.app_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.app_window->ui.letterbox_enabled = enabled;
  if (state.app_window->window.hwnd) {
    request_repaint(state);
  }
}

auto update_menu_items(Core::State::AppState& state) -> void {
  state.app_window->data.menu_items.clear();
  initialize_menu_items(state);
  if (state.app_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_menu_items_to_show(Core::State::AppState& state, std::span<const std::wstring> items)
    -> void {
  state.app_window->data.menu_items_to_show.assign(items.begin(), items.end());
}

auto register_toggle_visibility_hotkey(Core::State::AppState& state, UINT modifiers, UINT key)
    -> bool {
  if (state.app_window->window.hwnd) {
    return ::RegisterHotKey(state.app_window->window.hwnd,
                            state.app_window->window.toggle_visibility_hotkey_id, modifiers, key);
  }
  return false;
}

auto register_screenshot_hotkey(Core::State::AppState& state, UINT modifiers, UINT key) -> bool {
  if (state.app_window->window.hwnd) {
    return ::RegisterHotKey(state.app_window->window.hwnd,
                            state.app_window->window.screenshot_hotkey_id, modifiers, key);
  }
  return false;
}

auto unregister_hotkey(Core::State::AppState& state) -> void {
  if (state.app_window->window.hwnd) {
    ::UnregisterHotKey(state.app_window->window.hwnd,
                       state.app_window->window.toggle_visibility_hotkey_id);
    ::UnregisterHotKey(state.app_window->window.hwnd,
                       state.app_window->window.screenshot_hotkey_id);
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

auto initialize_menu_items(Core::State::AppState& state) -> void {
  state.app_window->data.menu_items.clear();

  // 通过统一的 MenuData API 获取所有数据
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  const auto& feature_items = Common::MenuData::get_current_feature_items(state);

  // 添加比例选项
  for (size_t i = 0; i < ratios.size(); ++i) {
    state.app_window->data.menu_items.emplace_back(
        ratios[i].name, UI::AppWindow::MenuItemCategory::AspectRatio, static_cast<int>(i));
  }

  // 添加分辨率选项
  for (size_t i = 0; i < resolutions.size(); ++i) {
    std::wstring displayText;
    const auto& preset = resolutions[i];
    displayText = preset.name;
    state.app_window->data.menu_items.emplace_back(
        displayText, UI::AppWindow::MenuItemCategory::Resolution, static_cast<int>(i));
  }

  // 添加功能项（使用新的统一API，现在文本已经通过i18n系统本地化了）
  for (size_t i = 0; i < feature_items.size(); ++i) {
    const auto& item = feature_items[i];
    state.app_window->data.menu_items.emplace_back(
        item.text, UI::AppWindow::MenuItemCategory::Feature, static_cast<int>(i), item.action_id);
  }
}

auto create_window_attributes(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// 设置变更响应实现
auto refresh_from_settings(Core::State::AppState& state) -> void {
  // 更新菜单项
  update_menu_items(state);

  // 更新布局配置（基于应用状态）
  UI::AppWindow::Layout::update_layout(state);

  // 更新颜色配置
  UI::AppWindow::D2DContext::update_all_brush_colors(state);

  // 重新计算窗口大小
  const auto new_size = UI::AppWindow::Layout::calculate_window_size(state);
  if (state.app_window->window.hwnd) {
    // 调整窗口大小
    SetWindowPos(state.app_window->window.hwnd, nullptr, 0, 0, new_size.cx, new_size.cy,
                 SWP_NOMOVE | SWP_NOZORDER);
    state.app_window->window.size = new_size;
    // 日志输出大小
    Logger().info("Window size updated: {}x{}", new_size.cx, new_size.cy);
  }

  state.app_window->d2d_context.needs_font_update = true;

  // 请求重绘
  request_repaint(state);
}

}  // namespace UI::AppWindow