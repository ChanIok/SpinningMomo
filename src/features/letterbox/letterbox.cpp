module;

#include <dwmapi.h>
#include <windows.h>

#include <iostream>
#include <thread>

module Features.Letterbox;

import std;
import Core.State;
import Features.Letterbox.State;

namespace Features::Letterbox {

// 全局状态指针，用于钩子回调
Core::State::AppState* g_app_state = nullptr;

// 更新位置
auto update_position(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (!letterbox.is_initialized) {
    return std::unexpected{"Letterbox not initialized"};
  }


  if (!letterbox.target_window) {
    return std::unexpected{"No target window specified"};
  }

  // 检查目标窗口是否有效
  if (!IsWindow(letterbox.target_window)) {
    auto hide_result = hide(state);
    return std::unexpected{"Target window is no longer valid"};
  }

  // 获取屏幕尺寸
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);

  // 设置letterbox窗口为全屏
  SetWindowPos(letterbox.window_handle, letterbox.target_window, 0, 0, screen_width, screen_height,
               SWP_NOACTIVATE);

  // 设置计时器处理任务栏置底
  SetTimer(letterbox.window_handle, State::TIMER_TASKBAR_ZORDER, 10, nullptr);

  return {};
}

// 静态回调函数实现
LRESULT CALLBACK letterbox_wnd_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Core::State::AppState* state = nullptr;

  // 添加WM_NCCREATE处理逻辑
  if (message == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  // 添加空状态检查
  if (!state) {
    return DefWindowProc(hwnd, message, wParam, lParam);
  }

  auto& letterbox = *state->letterbox;

  switch (message) {
    case WM_TIMER:
      if (wParam == State::TIMER_TASKBAR_ZORDER) {
        if (HWND taskbar = FindWindow(TEXT("Shell_TrayWnd"), NULL)) {
          SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        KillTimer(hwnd, State::TIMER_TASKBAR_ZORDER);
      }
      break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      if (letterbox.target_window && IsWindow(letterbox.target_window)) {
        SetForegroundWindow(letterbox.target_window);
        SetWindowPos(letterbox.window_handle, letterbox.target_window, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetTimer(letterbox.window_handle, State::TIMER_TASKBAR_ZORDER, 10, nullptr);
      }
      break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

auto register_window_class(HINSTANCE instance) -> std::expected<void, std::string> {
  WNDCLASSEX wcex = {0};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = letterbox_wnd_proc;
  wcex.hInstance = instance;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
  wcex.lpszClassName = L"LetterboxWindowClass";

  if (!RegisterClassEx(&wcex)) {
    return std::unexpected{
        std::format("Failed to register letterbox window class, error: {}", GetLastError())};
  }

  return {};
}

auto create_letterbox_window(State::LetterboxState& letterbox, Core::State::AppState* state)
    -> std::expected<void, std::string> {
  letterbox.window_handle = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                                            L"LetterboxWindowClass", L"Letterbox", WS_POPUP, 0, 0,
                                            0, 0, nullptr, nullptr, letterbox.instance, state);

  if (!letterbox.window_handle) {
    return std::unexpected{
        std::format("Failed to create letterbox window, error: {}", GetLastError())};
  }

  SetLayeredWindowAttributes(letterbox.window_handle, 0, 255, LWA_ALPHA);

  return {};
}

// 初始化
auto initialize(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (letterbox.is_initialized) {
    return std::unexpected{"Letterbox already initialized"};
  }

  // 设置全局状态指针
  g_app_state = &state;

  letterbox.instance = instance;
  letterbox.is_visible = false;

  // 注册窗口类
  if (auto result = register_window_class(instance); !result) {
    return result;
  }

  // 创建letterbox窗口
  if (auto result = create_letterbox_window(letterbox, &state); !result) {
    return result;
  }

  letterbox.is_initialized = true;
  return {};
}

LRESULT CALLBACK message_wnd_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Core::State::AppState* state = nullptr;

  // 添加WM_NCCREATE处理逻辑
  if (message == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  // 添加空状态检查
  if (!state) {
    return DefWindowProc(hwnd, message, wParam, lParam);
  }

  switch (message) {
    case State::WM_TARGET_WINDOW_FOREGROUND:
      if (!state->letterbox->is_visible) {
        [[maybe_unused]] auto result = show(*state);
      } else {
        [[maybe_unused]] auto result = update_position(*state, state->letterbox->target_window);
      }
      break;

    case State::WM_HIDE_LETTERBOX:
      if (state->letterbox->is_visible) {
        [[maybe_unused]] auto result = hide(*state);
      }
      break;

    case State::WM_SHOW_LETTERBOX:
      if (!state->letterbox->is_visible && state->letterbox->target_window) {
        [[maybe_unused]] auto result = show(*state);
      }
      break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                             LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
  // 使用全局状态指针替代窗口属性
  auto* state = g_app_state;

  if (!state || !state->letterbox->event_thread.joinable() || !state->letterbox->message_window) {
    return;
  }

  auto& letterbox = *state->letterbox;

  // 只处理与目标窗口相关的事件
  if (hwnd == letterbox.target_window) {
    switch (event) {
      case EVENT_SYSTEM_FOREGROUND:
        PostMessage(letterbox.message_window, State::WM_TARGET_WINDOW_FOREGROUND, 0, 0);
        break;

      case EVENT_SYSTEM_MINIMIZESTART:
        PostMessage(letterbox.message_window, State::WM_HIDE_LETTERBOX, 0, 0);
        break;

      case EVENT_OBJECT_DESTROY:
        PostMessage(letterbox.message_window, State::WM_HIDE_LETTERBOX, 0, 0);
        break;
    }
  }
}

auto event_thread_proc(Core::State::AppState& state, std::stop_token stoken,
                       const State::LetterboxConfig& config) -> void {
  auto& letterbox = *state.letterbox;

  // 注册消息窗口类
  WNDCLASSEX wcMessage = {0};
  wcMessage.cbSize = sizeof(WNDCLASSEX);
  wcMessage.lpfnWndProc = message_wnd_proc;
  wcMessage.hInstance = letterbox.instance;
  wcMessage.lpszClassName = L"SpinningMomoLetterboxMessageClass";

  if (!RegisterClassEx(&wcMessage)) {
    return;
  }

  // 创建消息窗口
  letterbox.message_window =
      CreateWindowExW(0, L"SpinningMomoLetterboxMessageClass", L"LetterboxMessage", WS_OVERLAPPED,
                      0, 0, 0, 0, HWND_MESSAGE, nullptr, letterbox.instance, &state);

  if (!letterbox.message_window) {
    UnregisterClass(L"SpinningMomoLetterboxMessageClass", letterbox.instance);
    return;
  }

  // 关联消息窗口与状态
  SetWindowLongPtr(letterbox.message_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

  // 获取目标窗口进程ID
  if (letterbox.target_window) {
    GetWindowThreadProcessId(letterbox.target_window, &letterbox.target_process_id);

    // 设置窗口事件钩子
    letterbox.event_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY, NULL,
                                           win_event_proc, letterbox.target_process_id, 0,
                                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  }

  // 消息循环
  MSG msg;
  while (!stoken.stop_requested() && GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // 清理资源
  if (letterbox.event_hook) {
    UnhookWinEvent(letterbox.event_hook);
    letterbox.event_hook = nullptr;
  }

  if (letterbox.message_window) {
    DestroyWindow(letterbox.message_window);
    letterbox.message_window = nullptr;
  }

  UnregisterClass(L"SpinningMomoLetterboxMessageClass", letterbox.instance);
}

// 启动事件监听
auto start_event_monitoring(Core::State::AppState& state, const State::LetterboxConfig& config)
    -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (letterbox.event_thread.joinable()) {
    return {};  // 已经在运行
  }

  try {
    letterbox.event_thread = std::jthread(
        [&state, config](std::stop_token stoken) { event_thread_proc(state, stoken, config); });
    return {};
  } catch (const std::exception& e) {
    return std::unexpected{std::format("Failed to start event thread: {}", e.what())};
  }
}

// 显示
auto show(Core::State::AppState& state, HWND target_window) -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  // 检查Letterbox是否已初始化，如果未初始化则进行初始化
  if (!letterbox.is_initialized) {
    HINSTANCE instance = GetModuleHandle(nullptr);
    if (auto result = initialize(state, instance); !result) {
      return std::unexpected{"Failed to initialize letterbox: " + result.error()};
    }
  }

  if (target_window) {
    letterbox.target_window = target_window;
  }

  if (!letterbox.target_window) {
    return std::unexpected{"No target window specified"};
  }

  // 检查目标窗口是否可见
  if (!IsWindowVisible(letterbox.target_window)) {
    return std::unexpected{"Target window is not visible"};
  }

  // 确保事件监听线程已启动
  if (!state.letterbox->event_thread.joinable()) {
    if (auto result = start_event_monitoring(state, State::LetterboxConfig{}); !result) {
      return std::unexpected{"Failed to start event monitoring: " + result.error()};
    }
  }

  ShowWindow(letterbox.window_handle, SW_SHOWNA);

  if (auto result = update_position(state, letterbox.target_window); !result) {
    return result;
  }

  letterbox.is_visible = true;
  return {};
}

// 隐藏
auto hide(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (!letterbox.is_initialized) {
    return std::unexpected{"Letterbox not initialized"};
  }

  if (letterbox.window_handle) {
    ShowWindow(letterbox.window_handle, SW_HIDE);
    letterbox.is_visible = false;
  }


  return {};
}

// 停止事件监听
auto stop_event_monitoring(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (letterbox.event_thread.joinable()) {
    letterbox.event_thread.request_stop();
    letterbox.event_thread.join();
  }

  if (letterbox.event_hook) {
    UnhookWinEvent(letterbox.event_hook);
    letterbox.event_hook = nullptr;
  }

  return {};
}

// 关闭
auto shutdown(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& letterbox = *state.letterbox;

  if (!letterbox.is_initialized) {
    return {};
  }

  // 隐藏窗口
  if (letterbox.is_visible) {
    [[maybe_unused]] auto hide_result = hide(state);
    // 记录错误但继续清理
  }

  // 停止事件监听
  [[maybe_unused]] auto stop_result = stop_event_monitoring(state);
  // 记录错误但继续清理


  // 销毁窗口
  if (letterbox.window_handle) {
    DestroyWindow(letterbox.window_handle);
    letterbox.window_handle = nullptr;
  }

  // 注销窗口类
  UnregisterClass(L"LetterboxWindowClass", letterbox.instance);

  // 清除全局状态指针
  g_app_state = nullptr;

  letterbox.is_initialized = false;

  return {};
}

}  // namespace Features::Letterbox