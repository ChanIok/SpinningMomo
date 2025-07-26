module;

#include <windows.h>

#include <iostream>
#include <thread>
// #include <winrt/base.h>

module Features.Overlay.Threads;

import std;
import Core.State;
import Features.Overlay.State;
import Features.Overlay.Types;
import Features.Overlay.Rendering;
import Features.Overlay.Capture;
import Features.Overlay.Interaction;
import Features.Overlay.Window;
import Utils.Logger;

namespace Features::Overlay::Threads {

auto start_threads(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 确保之前的线程已停止
  stop_threads(state);

  overlay_state.threads.should_stop = false;

  try {
    // 启动捕获和渲染线程
    overlay_state.threads.capture_render_thread =
        std::jthread([&state](std::stop_token token) { capture_render_thread_proc(state, token); });

    // 启动钩子线程
    overlay_state.threads.hook_thread =
        std::jthread([&state](std::stop_token token) { hook_thread_proc(state, token); });

    // 启动窗口管理线程
    overlay_state.threads.window_manager_thread =
        std::jthread([&state](std::stop_token token) { window_manager_thread_proc(state, token); });

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::format("Failed to start threads: {}", e.what()));
  }
}

auto stop_threads(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  overlay_state.threads.should_stop = true;
  overlay_state.running = false;  // 设置运行状态为false

  // 请求停止所有线程
  if (overlay_state.threads.capture_render_thread.joinable()) {
    overlay_state.threads.capture_render_thread.request_stop();
  }
  if (overlay_state.threads.hook_thread.joinable()) {
    overlay_state.threads.hook_thread.request_stop();
  }
  if (overlay_state.threads.window_manager_thread.joinable()) {
    overlay_state.threads.window_manager_thread.request_stop();
  }

  // 通知等待的线程
  overlay_state.frame_available.notify_all();
}

auto wait_for_threads(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (overlay_state.threads.capture_render_thread.joinable()) {
    Logger().debug("Waiting for capture and render thread to join");
    overlay_state.threads.capture_render_thread.join();
  }
  if (overlay_state.threads.hook_thread.joinable()) {
    Logger().debug("Waiting for hook thread to join");
    overlay_state.threads.hook_thread.join();
  }
  if (overlay_state.threads.window_manager_thread.joinable()) {
    Logger().debug("Waiting for window manager thread to join");
    overlay_state.threads.window_manager_thread.join();
  }
}

auto capture_render_thread_proc(Core::State::AppState& state, std::stop_token token) -> void {
  auto& overlay_state = *state.overlay;

  // 初始化COM
  // winrt::init_apartment();

  // 延迟防止闪烁
  if (Window::is_overlay_window_visible(state)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  if (token.stop_requested()) {
    return;
  }

  // 调整交换链大小
  if (auto result = Rendering::resize_swap_chain(state); !result) {
    return;
  }

  // 初始化渲染状态
  if (auto result = Rendering::initialize_render_states(state); !result) {
    return;
  }

  // 初始化捕获
  if (auto result = Capture::initialize_capture(state, overlay_state.window.target_window,
                                                overlay_state.window.cached_game_width,
                                                overlay_state.window.cached_game_height);
      !result) {
    return;
  }

  // 显示叠加层窗口
  PostMessage(overlay_state.window.overlay_hwnd, Types::WM_SHOW_OVERLAY, 0, 0);

  // 渲染循环
  while (!token.stop_requested() && !overlay_state.threads.should_stop) {
    // 等待新帧或超时
    std::unique_lock<std::mutex> lock(overlay_state.texture_mutex);
    overlay_state.frame_available.wait_for(lock, std::chrono::milliseconds(16), [&] {
      return Rendering::has_new_frame(state) || token.stop_requested() ||
             overlay_state.threads.should_stop;
    });

    if (token.stop_requested() || overlay_state.threads.should_stop) {
      break;
    }

    if (Rendering::has_new_frame(state)) {
      lock.unlock();

      // 执行渲染
      Rendering::render_frame(state, overlay_state.rendering.frame_texture);

      // 重置新帧标志
      Rendering::set_new_frame_flag(state, false);
    }
  }
}

auto hook_thread_proc(Core::State::AppState& state, std::stop_token token) -> void {
  // 初始化交互系统
  if (auto result = Interaction::initialize_interaction(state); !result) {
    return;
  }

  // 安装钩子
  if (auto result = Interaction::install_mouse_hook(state); !result) {
    return;
  }

  if (auto result = Interaction::install_window_event_hook(state); !result) {
    Interaction::uninstall_hooks(state);
    return;
  }

  // 改进的消息循环 - 使用PeekMessage避免阻塞
  MSG msg;
  while (!token.stop_requested() && !state.overlay->threads.should_stop) {
    BOOL result = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
    if (result) {
      if (msg.message == WM_QUIT) {
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // 没有消息时短暂休眠，避免过度占用CPU
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  // 清理钩子
  Interaction::uninstall_hooks(state);
}

auto window_manager_thread_proc(Core::State::AppState& state, std::stop_token token) -> void {
  auto& overlay_state = *state.overlay;

  // 创建定时器窗口
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = L"WindowManagerClass";

  if (!RegisterClassExW(&wc)) {
    return;
  }

  HWND timer_window = CreateWindowExW(0, L"WindowManagerClass", L"Timer Window", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);

  if (!timer_window) {
    UnregisterClassW(L"WindowManagerClass", GetModuleHandle(nullptr));
    return;
  }

  overlay_state.window.timer_window = timer_window;

  // 设置定时器
  SetTimer(timer_window, 1, 16, nullptr);  // ~60 FPS

  // 消息循环
  MSG msg;
  while (!token.stop_requested() && !overlay_state.threads.should_stop) {
    BOOL result = GetMessage(&msg, nullptr, 0, 0);
    if (result == -1 || result == 0) {
      break;
    }

    switch (msg.message) {
      case WM_TIMER:
        // 更新游戏窗口位置
        Interaction::update_game_window_position(state);
        break;

      case Types::WM_GAME_WINDOW_FOREGROUND:
        // 确保叠加层窗口在游戏窗口上方
        if (overlay_state.window.overlay_hwnd && overlay_state.window.target_window) {
          SetWindowPos(overlay_state.window.overlay_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
          SetWindowPos(overlay_state.window.overlay_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
          SetWindowPos(overlay_state.window.target_window, overlay_state.window.overlay_hwnd, 0, 0,
                       0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS |
                           SWP_ASYNCWINDOWPOS);
        }
        break;

      case Types::WM_MOUSE_EVENT: {
        // 处理鼠标事件
        POINT mouse_pos;
        mouse_pos.x = LOWORD(msg.wParam);
        mouse_pos.y = HIWORD(msg.wParam);
        Interaction::handle_mouse_movement(state, mouse_pos);
        break;
      }

      case Types::WM_WINDOW_EVENT: {
        // 处理窗口事件
        DWORD event = static_cast<DWORD>(msg.wParam);
        HWND hwnd = reinterpret_cast<HWND>(msg.lParam);
        Interaction::handle_window_event(state, event, hwnd);
        break;
      }
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // 清理资源
  KillTimer(timer_window, 1);
  DestroyWindow(timer_window);
  UnregisterClassW(L"WindowManagerClass", GetModuleHandle(nullptr));
  overlay_state.window.timer_window = nullptr;
}

auto are_threads_running(const Core::State::AppState& state) -> bool {
  const auto& overlay_state = *state.overlay;
  return overlay_state.threads.capture_render_thread.joinable() ||
         overlay_state.threads.hook_thread.joinable() ||
         overlay_state.threads.window_manager_thread.joinable();
}

}  // namespace Features::Overlay::Threads
