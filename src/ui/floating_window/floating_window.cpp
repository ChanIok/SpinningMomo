module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <string>

module UI.FloatingWindow;

import std;
import Features.Settings.Menu;
import Features.Settings.Types;
import Features.Settings.State;
import Core.Commands;
import Core.Commands.State;
import Core.Events;
import Core.State;
import Core.I18n.Types;
import Core.I18n.State;
import UI.FloatingWindow.Events;
import UI.FloatingWindow.MessageHandler;
import UI.FloatingWindow.Layout;
import UI.FloatingWindow.D2DContext;
import UI.FloatingWindow.Painter;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Types;
import Utils.Logger;
import Utils.String;

namespace UI::FloatingWindow {

auto create_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 获取系统DPI
  UINT dpi = 96;
  if (HDC hdc = GetDC(nullptr); hdc) {
    dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
  }

  // 保存DPI到状态中
  state.floating_window->window.dpi = dpi;

  // 应用布局配置（基于应用状态）
  UI::FloatingWindow::Layout::update_layout(state);

  // 初始化菜单项
  initialize_menu_items(state);

  // 计算窗口尺寸和位置
  const auto window_size = UI::FloatingWindow::Layout::calculate_window_size(state);
  const auto window_pos = UI::FloatingWindow::Layout::calculate_center_position(window_size);

  // 发送DPI改变事件来更新渲染状态
  Core::Events::send(*state.events, UI::FloatingWindow::Events::DpiChangeEvent{dpi, window_size});

  register_window_class(state.floating_window->window.instance);

  state.floating_window->window.hwnd = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"SpinningMomoAppWindowClass",
      L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN, window_pos.x, window_pos.y, window_size.cx,
      window_size.cy, nullptr, nullptr, state.floating_window->window.instance, &state);

  if (!state.floating_window->window.hwnd) {
    return std::unexpected("Failed to create window");
  }

  // 保存窗口尺寸和位置
  state.floating_window->window.size = window_size;
  state.floating_window->window.position = window_pos;

  // 创建窗口属性
  create_window_attributes(state.floating_window->window.hwnd);

  // 初始化Direct2D渲染
  if (!UI::FloatingWindow::D2DContext::initialize_d2d(state, state.floating_window->window.hwnd)) {
    Logger().error("Failed to initialize Direct2D rendering");
  }

  return {};
}

// 标准化渲染触发机制
auto request_repaint(Core::State::AppState& state) -> void {
  if (state.floating_window->window.hwnd && state.floating_window->window.is_visible) {
    InvalidateRect(state.floating_window->window.hwnd, nullptr, FALSE);
  }
}

auto show_window(Core::State::AppState& state) -> void {
  if (state.floating_window->window.hwnd) {
    ShowWindow(state.floating_window->window.hwnd, SW_SHOWNA);
    UpdateWindow(state.floating_window->window.hwnd);
    state.floating_window->window.is_visible = true;

    // 触发初始绘制（使用标准InvalidateRect机制）
    request_repaint(state);
  }
}

auto hide_window(Core::State::AppState& state) -> void {
  if (state.floating_window->window.hwnd) {
    ShowWindow(state.floating_window->window.hwnd, SW_HIDE);
    state.floating_window->window.is_visible = false;
  }
}

auto toggle_visibility(Core::State::AppState& state) -> void {
  if (state.floating_window->window.is_visible) {
    hide_window(state);
  } else {
    show_window(state);
  }
}

auto destroy_window(Core::State::AppState& state) -> void {
  unregister_hotkey(state);

  // 清理Direct2D资源
  UI::FloatingWindow::D2DContext::cleanup_d2d(state);

  if (state.floating_window->window.hwnd) {
    DestroyWindow(state.floating_window->window.hwnd);
    state.floating_window->window.hwnd = nullptr;
    state.floating_window->window.is_visible = false;
  }
}

auto set_current_ratio(Core::State::AppState& state, size_t index) -> void {
  state.floating_window->ui.current_ratio_index = index;
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_current_resolution(Core::State::AppState& state, size_t index) -> void {
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  if (index < resolutions.size()) {
    state.floating_window->ui.current_resolution_index = index;
    if (state.floating_window->window.hwnd) {
      request_repaint(state);
    }
  }
}

auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.floating_window->ui.preview_enabled = enabled;
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.floating_window->ui.overlay_enabled = enabled;
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void {
  state.floating_window->ui.letterbox_enabled = enabled;
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto update_menu_items(Core::State::AppState& state) -> void {
  state.floating_window->data.menu_items.clear();
  initialize_menu_items(state);
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_menu_items_to_show(Core::State::AppState& state, std::span<const std::wstring> items)
    -> void {
  state.floating_window->data.menu_items_to_show.assign(items.begin(), items.end());
}

auto register_toggle_visibility_hotkey(Core::State::AppState& state, UINT modifiers, UINT key)
    -> bool {
  if (state.floating_window->window.hwnd) {
    return ::RegisterHotKey(state.floating_window->window.hwnd,
                            state.floating_window->window.toggle_visibility_hotkey_id, modifiers,
                            key);
  }
  return false;
}

auto register_screenshot_hotkey(Core::State::AppState& state, UINT modifiers, UINT key) -> bool {
  if (state.floating_window->window.hwnd) {
    return ::RegisterHotKey(state.floating_window->window.hwnd,
                            state.floating_window->window.screenshot_hotkey_id, modifiers, key);
  }
  return false;
}

auto unregister_hotkey(Core::State::AppState& state) -> void {
  if (state.floating_window->window.hwnd) {
    ::UnregisterHotKey(state.floating_window->window.hwnd,
                       state.floating_window->window.toggle_visibility_hotkey_id);
    ::UnregisterHotKey(state.floating_window->window.hwnd,
                       state.floating_window->window.screenshot_hotkey_id);
  }
}

// 内部辅助函数实现

// 根据 i18n_key 获取本地化文本（扁平化版本）
auto get_text_by_i18n_key(const std::string& i18n_key, const Core::I18n::Types::TextData& texts)
    -> std::wstring {
  auto it = texts.find(i18n_key);
  if (it != texts.end()) {
    return Utils::String::FromUtf8(it->second);
  }
  // Fallback: 返回 key 本身
  return Utils::String::FromUtf8(i18n_key);
}

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
  state.floating_window->data.menu_items.clear();

  // 获取比例和分辨率预设
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);

  // 从配置获取功能项顺序
  const auto& feature_config = state.settings->raw.ui.app_menu.features;
  const auto& texts = state.i18n->texts;

  // 添加比例选项
  for (size_t i = 0; i < ratios.size(); ++i) {
    state.floating_window->data.menu_items.emplace_back(
        ratios[i].name, UI::FloatingWindow::MenuItemCategory::AspectRatio, static_cast<int>(i));
  }

  // 添加分辨率选项
  for (size_t i = 0; i < resolutions.size(); ++i) {
    const auto& preset = resolutions[i];
    state.floating_window->data.menu_items.emplace_back(
        preset.name, UI::FloatingWindow::MenuItemCategory::Resolution, static_cast<int>(i));
  }

  // 添加功能项（从命令注册表获取）
  if (state.commands) {
    for (size_t i = 0; i < feature_config.size(); ++i) {
      const auto& command_id = feature_config[i];
      // 从注册表获取命令描述
      if (auto command_opt = Core::Commands::get_command(state.commands->registry, command_id)) {
        // 使用 i18n_key 获取文本
        std::wstring text = get_text_by_i18n_key(command_opt->i18n_key, texts);
        state.floating_window->data.menu_items.emplace_back(
            text, UI::FloatingWindow::MenuItemCategory::Feature, static_cast<int>(i), command_id);
      } else {
        Logger().warn("Command not found in registry: {}", command_id);
      }
    }
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
  UI::FloatingWindow::Layout::update_layout(state);

  // 更新颜色配置
  UI::FloatingWindow::D2DContext::update_all_brush_colors(state);

  // 重新计算窗口大小
  const auto new_size = UI::FloatingWindow::Layout::calculate_window_size(state);
  if (state.floating_window->window.hwnd) {
    // 调整窗口大小
    SetWindowPos(state.floating_window->window.hwnd, nullptr, 0, 0, new_size.cx, new_size.cy,
                 SWP_NOMOVE | SWP_NOZORDER);
    state.floating_window->window.size = new_size;
    // 日志输出大小
    Logger().info("Window size updated: {}x{}", new_size.cx, new_size.cy);
  }

  state.floating_window->d2d_context.needs_font_update = true;

  // 请求重绘
  request_repaint(state);
}

}  // namespace UI::FloatingWindow