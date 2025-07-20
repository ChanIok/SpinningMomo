module;

#include <windows.h>

#include <iostream>

module UI.WebViewWindow;

import std;
import Core.State;
import Core.WebView.State;
import Utils.Logger;
import Vendor.Windows;

namespace UI::WebViewWindow {

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 注册窗口类
  register_window_class(state.app_window.window.instance);

  // 设置默认窗口大小和位置
  const int width = 1280;
  const int height = 720;
  const int x = 200;
  const int y = 100;

  // 创建独立窗口
  HWND hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,                 // 扩展样式
                              L"SpinningMomoWebViewWindowClass",      // 窗口类名
                              L"SpinningMomo WebView",                // 窗口标题
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,  // 窗口样式
                              x, y, width, height,                    // 位置和大小
                              nullptr,                                // 父窗口
                              nullptr,                                // 菜单
                              state.app_window.window.instance,       // 实例句柄
                              &state                                  // 用户数据
  );

  if (!hwnd) {
    return std::unexpected("Failed to create WebView window");
  }

  // 保存窗口句柄到WebView状态中
  state.webview->window.parent_hwnd = hwnd;
  state.webview->window.width = width;
  state.webview->window.height = height;
  state.webview->window.x = x;
  state.webview->window.y = y;
  state.webview->window.is_visible = false;

  Logger().info("WebView window created successfully");
  return {};
}

auto destroy(Core::State::AppState& state) -> void {
  if (state.webview->window.parent_hwnd) {
    DestroyWindow(state.webview->window.parent_hwnd);
    state.webview->window.parent_hwnd = nullptr;
    state.webview->window.is_visible = false;
    Logger().info("WebView window destroyed");
  }
}

auto show(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.webview->window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(state.webview->window.parent_hwnd, SW_SHOW);
  UpdateWindow(state.webview->window.parent_hwnd);
  state.webview->window.is_visible = true;

  Logger().info("WebView window shown");
  return {};
}

auto hide(Core::State::AppState& state) -> void {
  if (state.webview->window.parent_hwnd) {
    ShowWindow(state.webview->window.parent_hwnd, SW_HIDE);
    state.webview->window.is_visible = false;
    Logger().info("WebView window hidden");
  }
}

auto toggle_visibility(Core::State::AppState& state) -> void {
  if (state.webview->window.is_visible) {
    hide(state);
  } else {
    if (auto result = show(state); !result) {
      Logger().error("Failed to show WebView window: {}", result.error());
    }
  }
}

auto get_hwnd(const Core::State::AppState& state) -> Vendor::Windows::HWND {
  return state.webview->window.parent_hwnd;
}

auto is_visible(const Core::State::AppState& state) -> bool {
  return state.webview->window.is_visible;
}

auto register_window_class(Vendor::Windows::HINSTANCE instance) -> void {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"SpinningMomoWebViewWindowClass";
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  RegisterClassExW(&wc);
}

auto window_proc(Vendor::Windows::HWND hwnd, Vendor::Windows::UINT msg,
                 Vendor::Windows::WPARAM wparam, Vendor::Windows::LPARAM lparam)
    -> Vendor::Windows::LRESULT {
  Core::State::AppState* state = nullptr;

  if (msg == WM_NCCREATE) {
    // 获取创建参数中的状态指针
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    state = static_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    // 从窗口数据中获取状态指针
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case WM_SIZE: {
      if (state) {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        state->webview->window.width = width;
        state->webview->window.height = height;

        // 如果WebView已经初始化，同步调整大小
        if (state->webview->is_ready) {
          // 这里可以调用WebView的resize函数
          // Core::WebView::resize_webview(*state, width, height);
        }
      }
      break;
    }

    case WM_MOVE: {
      if (state) {
        int x = static_cast<int>(static_cast<short>(LOWORD(lparam)));
        int y = static_cast<int>(static_cast<short>(HIWORD(lparam)));
        state->webview->window.x = x;
        state->webview->window.y = y;
      }
      break;
    }

    case WM_CLOSE: {
      if (state) {
        // 隐藏窗口而不是销毁
        hide(*state);
        return 0;
      }
      break;
    }

    case WM_DESTROY: {
      if (state) {
        state->webview->window.is_visible = false;
      }
      break;
    }
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace UI::WebViewWindow