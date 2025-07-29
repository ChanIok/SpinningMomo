module;

#include <windows.h>

#include <iostream>

module Features.Overlay.Window;

import std;
import Features.Overlay.State;
import Utils.Logger;
import Core.State;
import Features.Overlay.Types;
import Features.Overlay.Utils;
import Features.Overlay.Interaction;

namespace Features::Overlay::Window {

// 窗口过程 - 使用GWLP_USERDATA模式获取状态
LRESULT CALLBACK overlay_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Core::State::AppState* state = nullptr;

  if (message == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (!state) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  // 调用交互模块处理消息
  auto [handled, result] =
      Interaction::handle_overlay_message(*state, hwnd, message, wParam, lParam);
  if (handled) {
    return result;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

auto register_overlay_window_class(HINSTANCE instance) -> std::expected<void, std::string> {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = overlay_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"OverlayWindowClass";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  if (!RegisterClassExW(&wc)) {
    DWORD error = GetLastError();
    return std::unexpected(
        std::format("Failed to register overlay window class. Error: {}", error));
  }

  return {};
}

auto unregister_overlay_window_class(HINSTANCE instance) -> void {
  UnregisterClassW(L"OverlayWindowClass", instance);
}

auto create_overlay_window(HINSTANCE instance, Core::State::AppState& state)
    -> std::expected<HWND, std::string> {
  auto& overlay_state = *state.overlay;
  auto [screen_width, screen_height] = Utils::get_screen_dimensions();

  overlay_state.window.screen_width = screen_width;
  overlay_state.window.screen_height = screen_height;

  HWND hwnd = CreateWindowExW(WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                                  WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
                              L"OverlayWindowClass", L"Overlay Window", WS_POPUP, 0, 0,
                              screen_width, screen_height, nullptr, nullptr, instance, &state);

  if (!hwnd) {
    DWORD error = GetLastError();
    auto error_msg = std::format("Failed to create overlay window. Error: {}", error);
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  overlay_state.window.overlay_hwnd = hwnd;
  Logger().info("Overlay window created successfully");
  return hwnd;
}

auto set_window_transparency(HWND hwnd, BYTE alpha) -> std::expected<void, std::string> {
  LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (!SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to set layered window style. Error: {}", error));
  }

  if (!SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to set window transparency. Error: {}", error));
  }

  return {};
}

auto set_window_layered_attributes(HWND hwnd) -> std::expected<void, std::string> {
  if (!SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)) {
    DWORD error = GetLastError();
    return std::unexpected(
        std::format("Failed to set layered window attributes. Error: {}", error));
  }
  return {};
}

auto initialize_overlay_window(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string> {
  // 注册窗口类
  if (auto result = register_overlay_window_class(instance); !result) {
    return std::unexpected(result.error());
  }

  // 创建窗口
  if (auto result = create_overlay_window(instance, state); !result) {
    unregister_overlay_window_class(instance);
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

  // 计算叠加层窗口尺寸
  if (overlay_state.window.use_letterbox_mode) {
    overlay_state.window.window_width = overlay_state.window.screen_width;
    overlay_state.window.window_height = overlay_state.window.screen_height;
  } else {
    auto [window_width, window_height] = Utils::calculate_overlay_dimensions(
        game_width, game_height, overlay_state.window.screen_width,
        overlay_state.window.screen_height);

    overlay_state.window.window_width = window_width;
    overlay_state.window.window_height = window_height;
  }

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

  SetForegroundWindow(overlay_state.window.target_window);
}

}  // namespace Features::Overlay::Window
