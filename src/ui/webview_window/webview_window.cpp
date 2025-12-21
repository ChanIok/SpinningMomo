module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

module UI.WebViewWindow;

import std;
import Core.State;
import Core.WebView;
import Core.WebView.State;
import UI.FloatingWindow.State;
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

  if (!state.webview->window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(state.webview->window.webview_hwnd, SW_SHOW);
  UpdateWindow(state.webview->window.webview_hwnd);
  state.webview->window.is_visible = true;

  Logger().info("WebView window shown");
  return {};
}

auto hide(Core::State::AppState& state) -> void {
  if (state.webview->window.webview_hwnd) {
    ShowWindow(state.webview->window.webview_hwnd, SW_HIDE);
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

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.webview_hwnd, SW_MINIMIZE);
  Logger().debug("WebView window minimized");
  return {};
}

auto maximize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.webview_hwnd, SW_MAXIMIZE);
  Logger().debug("WebView window maximized");
  return {};
}

auto restore_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.webview_hwnd, SW_RESTORE);
  Logger().debug("WebView window restored");
  return {};
}

auto close_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  // 发送WM_CLOSE消息关闭窗口
  PostMessage(webview_state.window.webview_hwnd, WM_CLOSE, 0, 0);
  Logger().debug("WebView window close requested");
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
    case WM_NCCALCSIZE: {
      // 处理非客户区大小计算，仅调整顶部边框以抵消WS_THICKFRAME的影响
      if (wparam == TRUE) {
        // 检查窗口是否已最大化
        bool is_maximized = IsZoomed(hwnd);

        // 只有在非最大化状态下才调整顶部边框
        if (!is_maximized) {
          NCCALCSIZE_PARAMS* nccsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);

          // 保存原始矩形
          RECT original = nccsp->rgrc[0];

          // 先调用默认处理，获取标准的客户区计算结果
          LRESULT result = DefWindowProcW(hwnd, msg, wparam, lparam);

          // 保持默认处理后的左侧、右侧和底部不变，只调整顶部
          nccsp->rgrc[0].top = original.top;

          return result;
        }
      }
      // 对于最大化窗口或其他情况，使用默认处理
      return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    case WM_GETMINMAXINFO: {
      // 限制窗口的最小尺寸为320x240
      MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
      mmi->ptMinTrackSize.x = 320;
      mmi->ptMinTrackSize.y = 240;
      return 0;
    }

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
  register_window_class(state.floating_window->window.instance);

  // 设置默认窗口大小和位置
  const int width = state.webview->window.width;
  const int height = state.webview->window.height;
  const int x = 200;
  const int y = 100;

  // 创建独立窗口
  HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,                         // 扩展样式
                              L"SpinningMomoWebViewWindowClass",       // 窗口类名
                              L"SpinningMomo WebView",                 // 窗口标题
                              WS_POPUP | WS_THICKFRAME,                // 窗口样式
                              x, y, width, height,                     // 位置和大小
                              nullptr,                                 // 父窗口
                              nullptr,                                 // 菜单
                              state.floating_window->window.instance,  // 实例句柄
                              &state                                   // 用户数据
  );

  if (!hwnd) {
    return std::unexpected("Failed to create WebView window");
  }

  // 设置窗口圆角
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  // 保存窗口句柄到WebView状态中
  state.webview->window.webview_hwnd = hwnd;
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

  if (state.webview->window.webview_hwnd) {
    DestroyWindow(state.webview->window.webview_hwnd);
    state.webview->window.webview_hwnd = nullptr;
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
  if (auto result = Core::WebView::initialize(state, state.webview->window.webview_hwnd); !result) {
    return std::unexpected("Failed to initialize WebView: " + result.error());
  }

  Logger().info("WebView window initialized");
  return {};
}

}  // namespace UI::WebViewWindow