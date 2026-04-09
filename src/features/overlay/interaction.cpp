module;

module Features.Overlay.Interaction;

import std;
import Core.State;
import Features.Overlay;
import Features.Overlay.Capture;
import Features.Overlay.Rendering;
import Features.Overlay.State;
import Features.Overlay.Types;
import Features.Overlay.Geometry;
import Utils.Logger;
import <dwmapi.h>;
import <windows.h>;

namespace Features::Overlay::Interaction {

// 全局状态指针，用于钩子回调
Core::State::AppState* g_app_state = nullptr;

// 窗口事件钩子过程。
// 这里故意不直接操作 overlay 状态，而是把事件转成窗口消息。
// 原因是 WinEventHook 回调运行在系统回调上下文里，逻辑越轻越安全；
// 真正的状态切换统一交给 overlay 自己的消息处理函数完成。
void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                             LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
  if (!g_app_state) {
    return;
  }

  auto& overlay_state = *g_app_state->overlay;

  // 前台切换事件仍然沿用旧路径：先投递到 timer_window，
  // 再由 window manager 线程更新焦点状态和窗口层级。
  if (event == EVENT_SYSTEM_FOREGROUND) {
    if (overlay_state.window.timer_window) {
      PostMessage(overlay_state.window.timer_window, Types::WM_WINDOW_EVENT,
                  static_cast<WPARAM>(event), reinterpret_cast<LPARAM>(hwnd));
    }
    return;
  }

  // 目标窗口被销毁时，不在 hook 回调里直接 stop_overlay()。
  // 而是通知 overlay 窗口自己处理，避免在回调线程里做复杂停机。
  if (event == EVENT_OBJECT_DESTROY && hwnd == overlay_state.window.target_window &&
      idObject == OBJID_WINDOW && idChild == CHILDID_SELF && overlay_state.window.overlay_hwnd) {
    PostMessage(overlay_state.window.overlay_hwnd, Types::WM_TARGET_WINDOW_DESTROYED, 0, 0);
  }
}

// 统一维护 overlay 的“当前是否由游戏/overlay 持有焦点”状态。
// 启动时和前台切换时都走这里，避免 direct-start 与 transform-start 行为不一致。
auto update_focus_state(Core::State::AppState& state, HWND hwnd) -> void {
  auto& overlay_state = *state.overlay;
  bool is_game_or_overlay =
      (hwnd == overlay_state.window.target_window || hwnd == overlay_state.window.overlay_hwnd);

  overlay_state.interaction.is_game_focused = is_game_or_overlay;

  if (is_game_or_overlay) {
    if (hwnd == overlay_state.window.target_window && overlay_state.window.overlay_hwnd) {
      PostMessage(overlay_state.window.overlay_hwnd, Types::WM_GAME_WINDOW_FOREGROUND, 0, 0);
    }
  } else {
    restore_taskbar_redraw(state);
  }
}

auto initialize_interaction(Core::State::AppState& state) -> std::expected<void, std::string> {
  g_app_state = &state;

  // 保存目标窗口所属进程 ID。
  // 后面注册“目标窗口销毁”hook 时会把监听范围限制到这个进程，
  // 避免收到无关进程的销毁噪声。
  if (state.overlay->window.target_window) {
    DWORD process_id;
    GetWindowThreadProcessId(state.overlay->window.target_window, &process_id);
    state.overlay->interaction.game_process_id = process_id;
  }

  // 钩子安装时前台窗口可能早已稳定，不会自动补发 EVENT_SYSTEM_FOREGROUND；
  // 启动阶段需要主动读取一次当前前台窗口来初始化焦点状态。
  refresh_focus_state(state);

  return {};
}

auto install_window_event_hook(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 这个 hook 监听全局前台切换。
  // overlay 需要知道“现在前台还是不是游戏/overlay”，
  // 这样才能决定是否压制任务栏重绘，以及是否调整窗口层级。
  if (!overlay_state.interaction.foreground_event_hook) {
    overlay_state.interaction.foreground_event_hook =
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, win_event_proc,
                        0, 0, WINEVENT_OUTOFCONTEXT);

    if (!overlay_state.interaction.foreground_event_hook) {
      DWORD error = GetLastError();
      auto error_msg = std::format("Failed to install foreground event hook. Error: {}", error);
      Logger().error(error_msg);
      return std::unexpected(error_msg);
    }
  }

  // 这个 hook 只监听目标进程里的“窗口对象销毁”。
  // 我们真正关心的是目标游戏窗口消失，此时 overlay 应该跟着退出。
  if (!overlay_state.interaction.target_window_event_hook) {
    overlay_state.interaction.target_window_event_hook =
        SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, win_event_proc,
                        overlay_state.interaction.game_process_id, 0,
                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (!overlay_state.interaction.target_window_event_hook) {
      if (overlay_state.interaction.foreground_event_hook) {
        UnhookWinEvent(overlay_state.interaction.foreground_event_hook);
        overlay_state.interaction.foreground_event_hook = nullptr;
      }

      DWORD error = GetLastError();
      auto error_msg = std::format("Failed to install target window event hook. Error: {}", error);
      Logger().error(error_msg);
      return std::unexpected(error_msg);
    }
  }

  Logger().info("Window event hooks installed successfully");
  return {};
}

auto uninstall_hooks(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (overlay_state.interaction.foreground_event_hook) {
    UnhookWinEvent(overlay_state.interaction.foreground_event_hook);
    overlay_state.interaction.foreground_event_hook = nullptr;
  }

  if (overlay_state.interaction.target_window_event_hook) {
    UnhookWinEvent(overlay_state.interaction.target_window_event_hook);
    overlay_state.interaction.target_window_event_hook = nullptr;
  }
}

auto suppress_taskbar_redraw(Core::State::AppState& state) -> void {
  if (state.overlay->interaction.taskbar_redraw_suppressed) {
    return;  // 已经禁止了，无需重复操作
  }

  HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr);
  if (taskbar) {
    SendMessage(taskbar, WM_SETREDRAW, FALSE, 0);
    state.overlay->interaction.taskbar_redraw_suppressed = true;
    Logger().debug("[Overlay] Taskbar redraw suppressed");
  }
}

auto restore_taskbar_redraw(Core::State::AppState& state) -> void {
  HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr);
  if (taskbar) {
    SendMessage(taskbar, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(taskbar, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    state.overlay->interaction.taskbar_redraw_suppressed = false;
  }
}

auto update_game_window_position(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (!overlay_state.window.target_window) return;

  POINT current_pos{};
  if (!GetCursorPos(&current_pos)) {
    return;
  }

  // 计算叠加层窗口的位置
  int overlay_left = (overlay_state.window.screen_width - overlay_state.window.window_width) / 2;
  int overlay_top = (overlay_state.window.screen_height - overlay_state.window.window_height) / 2;

  // 只有鼠标位于 overlay 显示区域内时，才根据鼠标位置“拖动”游戏窗口。
  // 这样用户把鼠标移到别处时，不会继续改游戏窗口位置。
  if (current_pos.x >= overlay_left &&
      current_pos.x <= (overlay_left + overlay_state.window.window_width) &&
      current_pos.y >= overlay_top &&
      current_pos.y <= (overlay_top + overlay_state.window.window_height)) {
    // 在黑边模式下，计算实际的游戏显示区域
    if (overlay_state.window.use_letterbox_mode) {
      // 使用工具函数计算黑边区域，与渲染部分保持一致
      auto [content_left, content_top, content_width, content_height] =
          Geometry::calculate_letterbox_area(
              overlay_state.window.screen_width, overlay_state.window.screen_height,
              overlay_state.window.cached_game_width, overlay_state.window.cached_game_height);

      // 检查鼠标是否在游戏显示区域内
      if (current_pos.x >= content_left && current_pos.x < (content_left + content_width) &&
          current_pos.y >= content_top && current_pos.y < (content_top + content_height)) {
        // 计算鼠标在游戏显示区域中的相对位置（0.0 到 1.0）
        double relative_x = (current_pos.x - content_left) / static_cast<double>(content_width);
        double relative_y = (current_pos.y - content_top) / static_cast<double>(content_height);

        // 使用缓存的游戏窗口尺寸计算新位置
        int new_game_x =
            static_cast<int>(-relative_x * overlay_state.window.cached_game_width + current_pos.x);
        int new_game_y =
            static_cast<int>(-relative_y * overlay_state.window.cached_game_height + current_pos.y);

        // 根据焦点状态禁用任务栏重绘
        if (overlay_state.interaction.is_game_focused) {
          suppress_taskbar_redraw(state);
        }

        POINT new_game_pos = {new_game_x, new_game_y};
        if (auto& last_pos = overlay_state.interaction.last_game_window_pos;
            last_pos && last_pos->x == new_game_pos.x && last_pos->y == new_game_pos.y) {
          return;
        }

        if (SetWindowPos(
                overlay_state.window.target_window, nullptr, new_game_x, new_game_y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING)) {
          overlay_state.interaction.last_game_window_pos = new_game_pos;
        }
      }
    } else {
      // 非黑边模式：整个 overlay 都是有效显示区，直接按整个 overlay 计算相对位置。
      double relative_x =
          (current_pos.x - overlay_left) / static_cast<double>(overlay_state.window.window_width);
      double relative_y =
          (current_pos.y - overlay_top) / static_cast<double>(overlay_state.window.window_height);

      // 使用缓存的游戏窗口尺寸计算新位置
      int new_game_x =
          static_cast<int>(-relative_x * overlay_state.window.cached_game_width + current_pos.x);
      int new_game_y =
          static_cast<int>(-relative_y * overlay_state.window.cached_game_height + current_pos.y);

      // 根据焦点状态禁用任务栏重绘
      if (overlay_state.interaction.is_game_focused) {
        suppress_taskbar_redraw(state);
      }

      POINT new_game_pos = {new_game_x, new_game_y};
      if (auto& last_pos = overlay_state.interaction.last_game_window_pos;
          last_pos && last_pos->x == new_game_pos.x && last_pos->y == new_game_pos.y) {
        return;
      }

      if (SetWindowPos(
              overlay_state.window.target_window, nullptr, new_game_x, new_game_y, 0, 0,
              SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING)) {
        overlay_state.interaction.last_game_window_pos = new_game_pos;
      }
    }
  }
}

auto handle_window_event(Core::State::AppState& state, DWORD event, HWND hwnd) -> void {
  if (event == EVENT_SYSTEM_FOREGROUND) {
    update_focus_state(state, hwnd);
  }
}

auto refresh_focus_state(Core::State::AppState& state) -> void {
  // 使用系统当前前台窗口做一次同步，供 direct-start 或其它无前台切换的路径复用。
  update_focus_state(state, GetForegroundWindow());
}

auto cleanup_interaction(Core::State::AppState& state) -> void {
  uninstall_hooks(state);
  state.overlay->interaction.last_game_window_pos.reset();
  restore_taskbar_redraw(state);  // 确保任务栏重绘被恢复
  g_app_state = nullptr;
}

auto handle_overlay_message(Core::State::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<bool, LRESULT> {
  auto& overlay_state = *state.overlay;

  // overlay 的“控制面板”。
  // 外部线程和系统回调尽量只投递消息，真正修改 overlay 状态统一在这里做。
  switch (message) {
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
      return {true, 1};
    }

    case Types::WM_SCHEDULE_OVERLAY_CLEANUP: {
      KillTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID);
      if (SetTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID, 3000, nullptr) == 0) {
        Logger().error("Failed to schedule delayed overlay cleanup");
        return {true, 0};
      }

      return {true, 1};
    }

    case Types::WM_CANCEL_OVERLAY_CLEANUP: {
      KillTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID);
      return {true, 1};
    }

    case Types::WM_IMMEDIATE_OVERLAY_CLEANUP: {
      KillTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID);
      Features::Overlay::Capture::cleanup_capture(state);
      Features::Overlay::Rendering::cleanup_rendering(state);
      Logger().info("Overlay cleaned up");
      return {true, 1};
    }

    case Types::WM_TARGET_WINDOW_DESTROYED: {
      Logger().info("Target window destroyed, stopping overlay");

      // 目标窗口已不存在，所以这里走“不恢复目标窗口”的 stop 变体。
      Features::Overlay::stop_overlay(state, false);

      return {true, 1};
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

    case WM_TIMER: {
      if (wParam != Types::OVERLAY_CLEANUP_TIMER_ID) {
        break;
      }

      KillTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID);
      Features::Overlay::Capture::cleanup_capture(state);
      Features::Overlay::Rendering::cleanup_rendering(state);
      Logger().info("Overlay cleaned up");
      return {true, 1};
    }

    case WM_DESTROY:
      KillTimer(hwnd, Types::OVERLAY_CLEANUP_TIMER_ID);
      return {true, 0};
  }

  return {false, 0};
}

}  // namespace Features::Overlay::Interaction
