module;

#include <windows.h>

#include <iostream>

module Features.Overlay.Interaction;

import std;
import Core.State;
import Features.Overlay.Rendering;
import Features.Overlay.State;
import Features.Overlay.Types;
import Utils.Logger;

namespace Features::Overlay::Interaction {

namespace {
// 全局状态指针，用于钩子回调
Core::State::AppState* g_app_state = nullptr;

// 鼠标钩子过程 - 简化版，只发送消息
LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wParam, LPARAM lParam) {
  if (code >= 0 && g_app_state) {
    auto* mouse_struct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    // 只发送消息到timer_window，不直接处理
    if (g_app_state->overlay->window.timer_window) {
      PostMessage(g_app_state->overlay->window.timer_window, Types::WM_MOUSE_EVENT,
                  MAKEWPARAM(mouse_struct->pt.x, mouse_struct->pt.y), 0);
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

// 窗口事件钩子过程 - 简化版，只发送消息
void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                             LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
  if (g_app_state) {
    // 只发送消息到timer_window，不直接处理
    if (g_app_state->overlay->window.timer_window) {
      PostMessage(g_app_state->overlay->window.timer_window, Types::WM_WINDOW_EVENT,
                  static_cast<WPARAM>(event), reinterpret_cast<LPARAM>(hwnd));
    }
  }
}
}  // namespace

auto initialize_interaction(Core::State::AppState& state) -> std::expected<void, std::string> {
  g_app_state = &state;

  // 获取游戏进程ID
  if (state.overlay->window.target_window) {
    DWORD process_id;
    GetWindowThreadProcessId(state.overlay->window.target_window, &process_id);
    state.overlay->interaction.game_process_id = process_id;
  }

  return {};
}

auto install_mouse_hook(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (overlay_state.interaction.mouse_hook) {
    return {};  // 已经安装
  }

  overlay_state.interaction.mouse_hook =
      SetWindowsHookEx(WH_MOUSE_LL, mouse_hook_proc, GetModuleHandle(nullptr), 0);

  if (!overlay_state.interaction.mouse_hook) {
    DWORD error = GetLastError();
    auto error_msg = std::format("Failed to install mouse hook. Error: {}", error);
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  Logger().info("Mouse hook installed successfully");
  return {};
}

auto install_window_event_hook(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (overlay_state.interaction.event_hook) {
    return {};  // 已经安装
  }

  overlay_state.interaction.event_hook =
      SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, win_event_proc,
                      overlay_state.interaction.game_process_id, 0, WINEVENT_OUTOFCONTEXT);

  if (!overlay_state.interaction.event_hook) {
    DWORD error = GetLastError();
    auto error_msg = std::format("Failed to install window event hook. Error: {}", error);
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  Logger().info("Window event hook installed successfully");
  return {};
}

auto uninstall_hooks(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (overlay_state.interaction.mouse_hook) {
    UnhookWindowsHookEx(overlay_state.interaction.mouse_hook);
    overlay_state.interaction.mouse_hook = nullptr;
  }

  if (overlay_state.interaction.event_hook) {
    UnhookWinEvent(overlay_state.interaction.event_hook);
    overlay_state.interaction.event_hook = nullptr;
  }
}

auto handle_mouse_movement(Core::State::AppState& state, POINT mouse_pos) -> void {
  auto& overlay_state = *state.overlay;
  overlay_state.interaction.current_mouse_pos = mouse_pos;
}

auto update_game_window_position(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (!overlay_state.window.target_window) return;

  POINT current_pos = overlay_state.interaction.current_mouse_pos;
  POINT last_pos = overlay_state.interaction.last_mouse_pos;

  // 只有当鼠标位置发生变化时才更新窗口位置
  if (current_pos.x == last_pos.x && current_pos.y == last_pos.y) {
    return;
  }

  // 计算叠加层窗口的位置
  int overlay_left = (overlay_state.window.screen_width - overlay_state.window.window_width) / 2;
  int overlay_top = (overlay_state.window.screen_height - overlay_state.window.window_height) / 2;

  // 检查鼠标是否在叠加层窗口范围内
  if (current_pos.x >= overlay_left &&
      current_pos.x <= (overlay_left + overlay_state.window.window_width) &&
      current_pos.y >= overlay_top &&
      current_pos.y <= (overlay_top + overlay_state.window.window_height)) {
    // 计算鼠标在叠加层窗口中的相对位置（0.0 到 1.0）
    double relative_x =
        (current_pos.x - overlay_left) / static_cast<double>(overlay_state.window.window_width);
    double relative_y =
        (current_pos.y - overlay_top) / static_cast<double>(overlay_state.window.window_height);

    // 使用缓存的游戏窗口尺寸计算新位置
    int new_game_x =
        static_cast<int>(-relative_x * overlay_state.window.cached_game_width + current_pos.x);
    int new_game_y =
        static_cast<int>(-relative_y * overlay_state.window.cached_game_height + current_pos.y);

    // 更新游戏窗口位置
    SetWindowPos(overlay_state.window.target_window, nullptr, new_game_x, new_game_y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
  }

  // 更新上一次的鼠标位置
  overlay_state.interaction.last_mouse_pos = current_pos;
}

auto handle_window_event(Core::State::AppState& state, DWORD event, HWND hwnd) -> void {
  auto& overlay_state = *state.overlay;

  if (event == EVENT_SYSTEM_FOREGROUND && hwnd == overlay_state.window.target_window) {
    // 游戏窗口获得焦点，发送消息确保叠加层在上方
    if (overlay_state.window.overlay_hwnd) {
      PostMessage(overlay_state.window.overlay_hwnd, Types::WM_GAME_WINDOW_FOREGROUND, 0, 0);
    }
  }
}

auto cleanup_interaction(Core::State::AppState& state) -> void {
  uninstall_hooks(state);
  g_app_state = nullptr;
}

auto handle_overlay_message(Core::State::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<bool, LRESULT> {
  auto& overlay_state = *state.overlay;

  switch (message) {
    case Types::WM_SHOW_OVERLAY: {
      ShowWindow(hwnd, SW_SHOWNA);

      // 计算窗口大小和位置
      int screenWidth = overlay_state.window.screen_width;
      int screenHeight = overlay_state.window.screen_height;
      int left = 0;
      int top = 0;
      int width = screenWidth;
      int height = screenHeight;

      // 如果不是黑边填充模式，则使用原本的居中计算
      if (!overlay_state.window.use_letterbox_mode) {
        width = overlay_state.window.window_width;
        height = overlay_state.window.window_height;
        left = (screenWidth - width) / 2;
        top = (screenHeight - height) / 2;
        Logger().debug("Not using letterbox mode, using window size: {}x{}", width, height);
      }
      Logger().debug("Using letterbox mode: {}", overlay_state.window.use_letterbox_mode);

      // 添加分层窗口样式
      if (overlay_state.window.target_window) {
        LONG exStyle = GetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE);
        SetWindowLong(overlay_state.window.target_window, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

        // 设置透明度 (透明度1，但仍可点击)
        SetLayeredWindowAttributes(overlay_state.window.target_window, 0, 1, LWA_ALPHA);
      }

      // 设置窗口位置和大小
      SetWindowPos(hwnd, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

      if (overlay_state.window.target_window) {
        SetWindowPos(overlay_state.window.target_window, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      }

      UpdateWindow(hwnd);
      overlay_state.window.is_visible = true;
      return {true, 0};
    }

    case Types::WM_GAME_WINDOW_FOREGROUND: {
      // 处理游戏窗口前台事件
      if (overlay_state.window.target_window) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        SetWindowPos(overlay_state.window.target_window, hwnd, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS |
                         SWP_ASYNCWINDOWPOS);
      }
      return {true, 0};
    }

    case WM_SIZE: {
      // 处理窗口大小变化
      if (!overlay_state.rendering.d3d_initialized) {
        return {true, 0};
      }

      // 更新窗口尺寸
      overlay_state.window.window_width = LOWORD(lParam);
      overlay_state.window.window_height = HIWORD(lParam);

      // 调整渲染系统大小
      if (auto result = Rendering::resize_rendering(state); !result) {
        Logger().error("Failed to resize overlay rendering: {}", result.error());
      }

      return {true, 0};
    }

    case WM_DESTROY:
      overlay_state.window.is_visible = false;
      return {true, 0};
  }

  return {false, 0};
}

}  // namespace Features::Overlay::Interaction
