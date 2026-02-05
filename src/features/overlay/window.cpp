module;

module Features.Overlay.Window;

import std;
import Features.Overlay.State;
import Utils.Logger;
import Core.State;
import Features.Overlay.Types;
import Features.Overlay.Geometry;
import Features.Overlay.Interaction;
import <dwmapi.h>;
import <windows.h>;

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
  auto [screen_width, screen_height] = Geometry::get_screen_dimensions();

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

auto hide_overlay_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (overlay_state.window.overlay_hwnd) {
    ShowWindow(overlay_state.window.overlay_hwnd, SW_HIDE);
  }

  overlay_state.window.overlay_window_shown = false;
}

auto set_overlay_window_size(Core::State::AppState& state, int game_width, int game_height)
    -> void {
  auto& overlay_state = *state.overlay;

  // 缓存游戏窗口尺寸
  overlay_state.window.cached_game_width = game_width;
  overlay_state.window.cached_game_height = game_height;

  // 计算叠加层窗口尺寸
  if (overlay_state.window.use_letterbox_mode) {
    overlay_state.window.window_width = overlay_state.window.screen_width;
    overlay_state.window.window_height = overlay_state.window.screen_height;
  } else {
    auto [window_width, window_height] = Geometry::calculate_overlay_dimensions(
        game_width, game_height, overlay_state.window.screen_width,
        overlay_state.window.screen_height);

    overlay_state.window.window_width = window_width;
    overlay_state.window.window_height = window_height;
  }

  // 计算窗口大小和位置 - 在letterbox模式下，overlay窗口始终是全屏的
  int screen_width = overlay_state.window.screen_width;
  int screen_height = overlay_state.window.screen_height;
  int left = 0;
  int top = 0;
  int width = screen_width;
  int height = screen_height;

  // 在非letterbox模式下使用居中窗口
  if (!overlay_state.window.use_letterbox_mode) {
    width = overlay_state.window.window_width;
    height = overlay_state.window.window_height;
    left = (screen_width - width) / 2;
    top = (screen_height - height) / 2;
    Logger().debug("Not using letterbox mode, using window size: {}x{}", width, height);
  } else {
    Logger().debug("Using letterbox mode, using full screen size: {}x{}", width, height);
  }

  SetWindowPos(overlay_state.window.overlay_hwnd, nullptr, left, top, width, height,
               SWP_NOZORDER | SWP_NOACTIVATE);

  if (overlay_state.window.target_window) {
    SetWindowPos(overlay_state.window.target_window, overlay_state.window.overlay_hwnd, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
  }
}

auto destroy_overlay_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (overlay_state.window.overlay_hwnd) {
    DestroyWindow(overlay_state.window.overlay_hwnd);
    overlay_state.window.overlay_hwnd = nullptr;
  }
}

auto show_overlay_window_first_time(Core::State::AppState& state)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 显示叠加层窗口
  ShowWindow(overlay_state.window.overlay_hwnd, SW_SHOWNA);

  // 添加分层窗口样式到目标窗口
  LONG exStyle = GetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE);
  SetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

  // 设置透明度 (透明度1，但仍可点击)
  if (!SetLayeredWindowAttributes(overlay_state.window.target_window, 0, 1, LWA_ALPHA)) {
    DWORD error = GetLastError();
    return std::unexpected(
        std::format("Failed to set layered window attributes. Error: {}", error));
  }

  Logger().debug("Set target window layered");
  return {};
}

auto restore_game_window(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  if (!overlay_state.window.target_window) return;

  // 移除分层窗口样式
  LONG ex_style = GetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE);
  SetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE, ex_style & ~WS_EX_LAYERED);

  // 获取窗口尺寸
  auto dimensions_result = Geometry::get_window_dimensions(overlay_state.window.target_window);
  if (!dimensions_result) {
    return;
  }

  auto [width, height] = dimensions_result.value();

  int left = (overlay_state.window.screen_width - width) / 2;
  int top = (overlay_state.window.screen_height - height) / 2;
  SetWindowPos(overlay_state.window.target_window, HWND_TOP, left, top, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);

  SetForegroundWindow(overlay_state.window.target_window);
}

}  // namespace Features::Overlay::Window
