module;

module UI.WebViewWindow;

import std;
import Core.State;
import Core.WebView;
import Core.WebView.State;
import Features.Settings;
import Features.Settings.State;
import Features.Settings.Types;
import UI.FloatingWindow.State;
import UI.TrayIcon.Types;
import Utils.Logger;
import Vendor.Windows;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

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
      // 处理非客户区大小计算，实现无标题栏窗口
      // wparam == TRUE: 窗口大小改变时，lparam 指向 NCCALCSIZE_PARAMS
      // wparam == FALSE: 窗口创建时，lparam 指向 RECT
      if (wparam == TRUE) {
        NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);

        if (IsZoomed(hwnd)) {
          // 最大化：限制到工作区（不覆盖任务栏）
          HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
          MONITORINFO mi = {sizeof(mi)};
          if (GetMonitorInfoW(monitor, &mi)) {
            params->rgrc[0] = mi.rcWork;
          }
        } else {
          // 非最大化：获取系统计算的边框大小，但移除顶部标题栏
          RECT original = params->rgrc[0];
          DefWindowProcW(hwnd, msg, wparam, lparam);
          // 保留左右底边框，只移除顶部标题栏
          params->rgrc[0].top = original.top;
        }
      }
      // 返回 0 表示我们已处理此消息
      return 0;
    }

    case WM_GETMINMAXINFO: {
      MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lparam);

      // 设置最小尺寸
      mmi->ptMinTrackSize.x = 320;
      mmi->ptMinTrackSize.y = 240;

      // 设置最大化时的位置和尺寸（工作区，不覆盖任务栏）
      HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = {sizeof(mi)};
      if (GetMonitorInfoW(monitor, &mi)) {
        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
        mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
        mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
        mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
      }
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
  // 大图标：Alt+Tab、窗口标题栏等
  wc.hIcon = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(UI::TrayIcon::Types::IDI_ICON1), IMAGE_ICON,
                 GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
  if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

  // 小图标：任务栏
  wc.hIconSm = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(UI::TrayIcon::Types::IDI_ICON1), IMAGE_ICON,
                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  if (!wc.hIconSm) wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  RegisterClassExW(&wc);
}

auto apply_window_style(HWND hwnd) -> void {
  // 设置 Win11 圆角样式
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  // 强制触发 WM_NCCALCSIZE 重新计算边框
  // 这确保窗口创建后立即应用正确的非客户区计算
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 注册窗口类
  register_window_class(state.floating_window->window.instance);

  // 从设置读取持久化的尺寸和位置，默认 1200×800，位置居中
  int width = state.settings->raw.ui.webview_window.width;
  int height = state.settings->raw.ui.webview_window.height;
  int x = state.settings->raw.ui.webview_window.x;
  int y = state.settings->raw.ui.webview_window.y;

  // 限制在合理范围内（最小 320×240，最大为工作区）
  constexpr int kMinWidth = 320;
  constexpr int kMinHeight = 240;
  width = std::max(kMinWidth, width);
  height = std::max(kMinHeight, height);

  HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi = {sizeof(mi)};
  if (GetMonitorInfoW(monitor, &mi)) {
    int work_width = mi.rcWork.right - mi.rcWork.left;
    int work_height = mi.rcWork.bottom - mi.rcWork.top;
    width = std::min(width, work_width);
    height = std::min(height, work_height);
  }

  // 位置：x/y < 0 表示未保存过，居中；否则使用保存的位置
  // 若保存的位置完全不在当前显示器上，则回退到居中
  bool use_center = (x < 0 || y < 0);
  if (!use_center && GetMonitorInfoW(monitor, &mi)) {
    RECT work = mi.rcWork;
    // 窗口至少有一部分在工作区内
    bool visible =
        (x + width > work.left && x < work.right && y + height > work.top && y < work.bottom);
    if (!visible) {
      use_center = true;
    }
  }
  if (use_center && GetMonitorInfoW(monitor, &mi)) {
    int work_width = mi.rcWork.right - mi.rcWork.left;
    int work_height = mi.rcWork.bottom - mi.rcWork.top;
    x = mi.rcWork.left + (work_width - width) / 2;
    y = mi.rcWork.top + (work_height - height) / 2;
  }

  // 创建独立窗口
  // 窗口样式：
  // - WS_POPUP: 无边框窗口
  // - WS_THICKFRAME: 支持边缘拖拽调整大小
  // - WS_CAPTION: 启用 DWM 动画效果（标题栏通过 WM_NCCALCSIZE 隐藏）
  // - WS_SYSMENU: 启用系统菜单（Alt+Space）
  // - WS_MAXIMIZEBOX/WS_MINIMIZEBOX: 支持最大化/最小化
  constexpr DWORD style =
      WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
  HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,                         // 扩展样式
                              L"SpinningMomoWebViewWindowClass",       // 窗口类名
                              L"SpinningMomo WebView",                 // 窗口标题
                              style,                                   // 窗口样式
                              x, y, width, height,                     // 位置和大小
                              nullptr,                                 // 父窗口
                              nullptr,                                 // 菜单
                              state.floating_window->window.instance,  // 实例句柄
                              &state                                   // 用户数据
  );

  if (!hwnd) {
    return std::unexpected("Failed to create WebView window");
  }

  // 保存窗口句柄到 WebView 状态中
  state.webview->window.webview_hwnd = hwnd;
  state.webview->window.width = width;
  state.webview->window.height = height;
  state.webview->window.x = x;
  state.webview->window.y = y;
  state.webview->window.is_visible = false;

  // 应用窗口样式（圆角 + 触发边框重算）
  apply_window_style(hwnd);

  Logger().info("WebView window created successfully");
  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  // 关闭 WebView
  Core::WebView::shutdown(state);

  if (state.webview->window.webview_hwnd) {
    HWND hwnd = state.webview->window.webview_hwnd;

    // 持久化窗口尺寸和位置（若最大化则保存恢复后尺寸和位置）
    int width_to_save = state.webview->window.width;
    int height_to_save = state.webview->window.height;
    int x_to_save = state.webview->window.x;
    int y_to_save = state.webview->window.y;
    if (IsZoomed(hwnd)) {
      WINDOWPLACEMENT wp = {sizeof(wp)};
      if (GetWindowPlacement(hwnd, &wp)) {
        width_to_save = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        height_to_save = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        x_to_save = wp.rcNormalPosition.left;
        y_to_save = wp.rcNormalPosition.top;
      }
    }

    constexpr int kMinWidth = 320;
    constexpr int kMinHeight = 240;
    width_to_save = std::max(kMinWidth, width_to_save);
    height_to_save = std::max(kMinHeight, height_to_save);

    auto old_settings = state.settings->raw;
    state.settings->raw.ui.webview_window.width = width_to_save;
    state.settings->raw.ui.webview_window.height = height_to_save;
    state.settings->raw.ui.webview_window.x = x_to_save;
    state.settings->raw.ui.webview_window.y = y_to_save;

    auto settings_path = Features::Settings::get_settings_path();
    if (settings_path) {
      if (auto save_result =
              Features::Settings::save_settings_to_file(settings_path.value(), state.settings->raw);
          !save_result) {
        Logger().warn("Failed to persist WebView window bounds: {}", save_result.error());
      } else {
        Features::Settings::notify_settings_changed(state, old_settings,
                                                    "Settings updated via WebView window bounds");
      }
    }

    DestroyWindow(hwnd);
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
