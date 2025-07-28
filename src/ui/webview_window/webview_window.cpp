module;

#include <dwmapi.h>
#include <windows.h>

#include <iostream>

module UI.WebViewWindow;

import std;
import Core.State;
import Core.WebView;
import Core.WebView.State;
import UI.AppWindow.State;
import Utils.Logger;
import Vendor.Windows;

namespace UI::WebViewWindow {

auto show(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 如果 WebView 还未初始化，则进行初始化
  if (!state.webview->is_initialized) {
    if (auto result = initialize(state); !result) {
      return std::unexpected(result.error());
    }
  }

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

// 窗口控制功能实现
auto minimize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.parent_hwnd, SW_MINIMIZE);
  Logger().debug("WebView window minimized");
  return {};
}

auto maximize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.parent_hwnd, SW_MAXIMIZE);
  Logger().debug("WebView window maximized");
  return {};
}

auto restore_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.parent_hwnd, SW_RESTORE);
  Logger().debug("WebView window restored");
  return {};
}

auto close_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  // 发送WM_CLOSE消息关闭窗口
  PostMessage(webview_state.window.parent_hwnd, WM_CLOSE, 0, 0);
  Logger().debug("WebView window close requested");
  return {};
}

// 拖拽功能实现
auto start_drag_window(Core::State::AppState& state, int screen_x, int screen_y)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd) {
    return std::unexpected("WebView window not created");
  }

  // 设置拖拽状态
  webview_state.window.drag_state.is_dragging = true;
  webview_state.window.drag_state.start_pos.x = screen_x;
  webview_state.window.drag_state.start_pos.y = screen_y;

  // 获取当前窗口位置和大小
  GetWindowRect(webview_state.window.parent_hwnd, &webview_state.window.drag_state.window_rect);

  Logger().debug("Started dragging window from ({}, {})", screen_x, screen_y);
  return {};
}

auto move_drag_window(Core::State::AppState& state, int screen_x, int screen_y)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.parent_hwnd || !webview_state.window.drag_state.is_dragging) {
    return std::unexpected("Drag operation not active");
  }

  // 计算新的窗口位置
  int new_x = webview_state.window.drag_state.window_rect.left +
              (screen_x - webview_state.window.drag_state.start_pos.x);
  int new_y = webview_state.window.drag_state.window_rect.top +
              (screen_y - webview_state.window.drag_state.start_pos.y);

  // 更新窗口位置
  if (!SetWindowPos(webview_state.window.parent_hwnd, nullptr, new_x, new_y, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
    Logger().warn("Failed to move window during drag");
  }

  return {};
}

auto end_drag_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (webview_state.window.drag_state.is_dragging) {
    webview_state.window.drag_state.is_dragging = false;
    Logger().debug("Finished dragging window");
  }

  return {};
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
          // 调用WebView的resize函数来调整WebView控件大小
          Core::WebView::resize_webview(*state, width, height);
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
        cleanup(*state);
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

auto create_window_attributes(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 注册窗口类
  register_window_class(state.app_window->window.instance);

  // 设置默认窗口大小和位置
  const int width = 1280;
  const int height = 720;
  const int x = 200;
  const int y = 100;

  // 创建独立窗口（真正无边框样式）
  HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,                    // 扩展样式
                              L"SpinningMomoWebViewWindowClass",  // 窗口类名
                              L"SpinningMomo WebView",            // 窗口标题
                              WS_POPUP,                           // 真正的无边框窗口样式
                              x, y, width, height,                // 位置和大小
                              nullptr,                            // 父窗口
                              nullptr,                            // 菜单
                              state.app_window->window.instance,  // 实例句柄
                              &state                              // 用户数据
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

  // 创建窗口属性（Win11样式）
  create_window_attributes(hwnd);

  Logger().info("WebView window created successfully");
  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  // 关闭 WebView
  Core::WebView::shutdown(state);

  if (state.webview->window.parent_hwnd) {
    DestroyWindow(state.webview->window.parent_hwnd);
    state.webview->window.parent_hwnd = nullptr;
    state.webview->window.is_visible = false;
    Logger().info("WebView window destroyed");
  }
}

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 创建窗口
  if (auto result = create(state); !result) {
    return std::unexpected("Failed to create WebView window: " + result.error());
  }

  // 初始化 WebView
  if (auto result = Core::WebView::initialize(state, state.webview->window.parent_hwnd); !result) {
    return std::unexpected("Failed to initialize WebView: " + result.error());
  }

  Logger().info("WebView window initialized");
  return {};
}

}  // namespace UI::WebViewWindow