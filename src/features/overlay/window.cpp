module;

#include <windows.h>

#include <iostream>

module Features.Overlay.Window;

import std;
import Features.Overlay.State;
import Features.Overlay.Utils;
import Utils.Logger;
import Core.State;

namespace Features::Overlay::Window {

auto create_overlay_window(HINSTANCE instance, HWND parent, Core::State::AppState& state)
    -> std::expected<HWND, std::string> {
  auto& overlay_state = *state.overlay;
  auto [screen_width, screen_height] = Utils::get_screen_dimensions();

  overlay_state.window.screen_width = screen_width;
  overlay_state.window.screen_height = screen_height;
  overlay_state.window.main_window = parent;

  HWND hwnd = CreateWindowExW(WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                                  WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
                              L"OverlayWindowClass", L"Overlay Window", WS_POPUP, 0, 0,
                              screen_width, screen_height, nullptr, nullptr, instance, nullptr);

  if (!hwnd) {
    DWORD error = GetLastError();
    auto error_msg = std::format("Failed to create overlay window. Error: {}", error);
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 设置窗口为完全透明但可见
  if (auto result = Utils::set_window_layered_attributes(hwnd); !result) {
    Logger().error(std::format("Failed to set window layered attributes: {}", result.error()));
    DestroyWindow(hwnd);
    return std::unexpected(result.error());
  }

  overlay_state.window.overlay_hwnd = hwnd;
  Logger().info("Overlay window created successfully");
  return hwnd;
}

auto initialize_overlay_window(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string> {
  // 注册窗口类
  if (auto result = Utils::register_overlay_window_class(instance); !result) {
    return std::unexpected(result.error());
  }

  // 创建窗口
  if (auto result = create_overlay_window(instance, parent, state); !result) {
    Utils::unregister_overlay_window_class(instance);
    return std::unexpected(result.error());
  }

  return {};
}

auto show_overlay_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (overlay_state.window.overlay_hwnd) {
    ShowWindow(overlay_state.window.overlay_hwnd, SW_SHOW);
    overlay_state.window.is_visible = true;
  }
}

auto hide_overlay_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (overlay_state.window.overlay_hwnd) {
    ShowWindow(overlay_state.window.overlay_hwnd, SW_HIDE);
    overlay_state.window.is_visible = false;
  }
}

auto is_overlay_window_visible(const Core::State::AppState& state) -> bool {
  const auto& overlay_state = *state.overlay;
  return overlay_state.window.is_visible && overlay_state.window.overlay_hwnd &&
         IsWindowVisible(overlay_state.window.overlay_hwnd);
}

auto update_overlay_window_size(Core::State::AppState& state, int game_width, int game_height)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 缓存游戏窗口尺寸
  overlay_state.window.cached_game_width = game_width;
  overlay_state.window.cached_game_height = game_height;

  // 检查是否需要叠加层
  if (!Utils::should_use_overlay(game_width, game_height, overlay_state.window.screen_width,
                                 overlay_state.window.screen_height)) {
    Logger().debug("Game window fits within screen, overlay not needed");
    return std::unexpected("Game window fits within screen, overlay not needed");
  }

  // 计算叠加层窗口尺寸
  auto [window_width, window_height] = Utils::calculate_overlay_dimensions(
      game_width, game_height, overlay_state.window.screen_width,
      overlay_state.window.screen_height);

  overlay_state.window.window_width = window_width;
  overlay_state.window.window_height = window_height;

  return {};
}

auto destroy_overlay_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (overlay_state.window.overlay_hwnd) {
    DestroyWindow(overlay_state.window.overlay_hwnd);
    overlay_state.window.overlay_hwnd = nullptr;
    overlay_state.window.is_visible = false;
  }
}

auto restore_game_window(Core::State::AppState& state, bool with_delay) -> void {
  auto& overlay_state = *state.overlay;
  if (!overlay_state.window.target_window) return;

  if (with_delay) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 移除分层窗口样式
  LONG ex_style = GetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE);
  SetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE, ex_style & ~WS_EX_LAYERED);

  // 重绘窗口
  RedrawWindow(overlay_state.window.target_window, nullptr, nullptr,
               RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

}  // namespace Features::Overlay::Window
