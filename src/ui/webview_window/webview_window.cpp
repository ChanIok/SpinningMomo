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
import Vendor.ShellApi;
import Core.State.RuntimeInfo;
import Core.HttpServer.State;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

namespace UI::WebViewWindow {

auto is_transparent_background_enabled(Core::State::AppState& state) -> bool {
  if (!state.settings) {
    return false;
  }
  return state.settings->raw.ui.webview_window.enable_transparent_background;
}

auto desired_window_ex_style(Core::State::AppState& state) -> DWORD {
  DWORD ex_style = WS_EX_APPWINDOW;
  if (is_transparent_background_enabled(state)) {
    ex_style |= WS_EX_NOREDIRECTIONBITMAP;
  }
  return ex_style;
}

auto apply_window_ex_style_from_settings(Core::State::AppState& state) -> void {
  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    return;
  }

  auto current_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
  auto updated_style = current_style;
  if (is_transparent_background_enabled(state)) {
    updated_style |= WS_EX_NOREDIRECTIONBITMAP;
  } else {
    updated_style &= ~WS_EX_NOREDIRECTIONBITMAP;
  }

  if (updated_style == current_style) {
    return;
  }

  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(updated_style));
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

auto default_window_style() -> DWORD { return WS_OVERLAPPEDWINDOW; }

auto fullscreen_window_style(DWORD base_style) -> DWORD {
  return base_style & ~(WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
}

auto get_window_rect_or_fallback(HWND hwnd, RECT fallback_rect) -> RECT {
  RECT rect = fallback_rect;
  GetWindowRect(hwnd, &rect);
  return rect;
}

struct WindowFrameInsets {
  int x = 0;
  int y = 0;
};

auto get_window_frame_insets_for_dpi(UINT dpi) -> WindowFrameInsets {
  auto frame_x = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi);
  auto frame_y = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi);
  auto padded_border = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

  return WindowFrameInsets{
      .x = frame_x + padded_border,
      .y = frame_y + padded_border,
  };
}

auto send_window_state_changed_notification(Core::State::AppState& state) -> void {
  auto payload = std::format(
      R"({{"jsonrpc":"2.0","method":"window.stateChanged","params":{{"maximized":{},"fullscreen":{}}}}})",
      state.webview->window.is_maximized ? "true" : "false",
      state.webview->window.is_fullscreen ? "true" : "false");
  Core::WebView::post_message(state, payload);
}

auto sync_window_state(Core::State::AppState& state, bool notify) -> void {
  if (!state.webview) {
    return;
  }

  auto& window = state.webview->window;
  auto hwnd = window.webview_hwnd;
  auto is_maximized = hwnd && IsZoomed(hwnd) == TRUE;

  if (window.is_maximized == is_maximized) {
    return;
  }

  window.is_maximized = is_maximized;
  Logger().debug("WebView window maximize state changed: {}", is_maximized);

  if (notify) {
    send_window_state_changed_notification(state);
  }
}

auto should_paint_loading_background(Core::State::AppState* state) -> bool {
  return state && state->webview && !state->webview->has_initial_content;
}

auto paint_loading_background(Core::State::AppState& state, HDC hdc, const RECT& rect) -> void {
  auto background = CreateSolidBrush(Core::WebView::get_loading_background_color(state));
  FillRect(hdc, &rect, background);
  DeleteObject(background);
}

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

auto activate_window(Core::State::AppState& state) -> void {
  if (state.runtime_info && !state.runtime_info->is_webview2_available) {
    Logger().warn("WebView2 runtime is unavailable. Opening in browser.");
    std::string url = std::format("http://localhost:{}/", state.http_server->port);
    std::wstring wurl(url.begin(), url.end());  // URL is ASCII

    Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{.cbSize = sizeof(exec_info),
                                                  .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,
                                                  .lpVerb = L"open",
                                                  .lpFile = wurl.c_str(),
                                                  .nShow = Vendor::ShellApi::kSW_SHOWNORMAL};
    Vendor::ShellApi::ShellExecuteExW(&exec_info);
    return;
  }

  if (auto result = show(state); !result) {
    Logger().error("Failed to activate WebView window: {}", result.error());
    return;
  }

  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    Logger().error("Failed to activate WebView window: WebView window not created");
    return;
  }

  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  } else {
    ShowWindow(hwnd, SW_SHOW);
  }

  SetForegroundWindow(hwnd);
  state.webview->window.is_visible = true;
  Logger().info("WebView window activated");
}

// Window control helpers
auto minimize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.webview_hwnd, SW_MINIMIZE);
  Logger().debug("WebView window minimized");
  return {};
}

auto toggle_maximize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  if (webview_state.window.is_fullscreen) {
    if (auto result = set_fullscreen_window(state, false); !result) {
      return result;
    }
  }

  auto hwnd = webview_state.window.webview_hwnd;
  auto was_maximized = IsZoomed(hwnd) == TRUE;
  ShowWindow(hwnd, was_maximized ? SW_RESTORE : SW_MAXIMIZE);

  Logger().debug("WebView window toggled maximize state");
  return {};
}

auto set_fullscreen_window(Core::State::AppState& state, bool fullscreen)
    -> std::expected<void, std::string> {
  auto& window = state.webview->window;
  auto hwnd = window.webview_hwnd;
  if (!hwnd) {
    return std::unexpected("WebView window not created");
  }

  if (window.is_fullscreen == fullscreen) {
    return {};
  }

  if (fullscreen) {
    window.fullscreen_restore_placement = WINDOWPLACEMENT{sizeof(WINDOWPLACEMENT)};
    if (!GetWindowPlacement(hwnd, &window.fullscreen_restore_placement)) {
      return std::unexpected("Failed to get WebView window placement");
    }

    window.fullscreen_restore_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    window.has_fullscreen_restore_state = true;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
      return std::unexpected("Failed to get monitor info for WebView window");
    }

    SetWindowLongPtrW(
        hwnd, GWL_STYLE,
        static_cast<LONG_PTR>(fullscreen_window_style(window.fullscreen_restore_style)));
    SetWindowPos(hwnd, HWND_TOPMOST, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    window.is_fullscreen = true;
    sync_window_state(state, false);
    send_window_state_changed_notification(state);
    Logger().debug("WebView window entered fullscreen");
    return {};
  }

  if (!window.has_fullscreen_restore_state) {
    return std::unexpected("WebView fullscreen restore state is unavailable");
  }

  SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(window.fullscreen_restore_style));
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  if (!SetWindowPlacement(hwnd, &window.fullscreen_restore_placement)) {
    return std::unexpected("Failed to restore WebView window placement");
  }

  auto show_cmd = window.fullscreen_restore_placement.showCmd;
  ShowWindow(hwnd, show_cmd == SW_SHOWMINIMIZED ? SW_RESTORE : show_cmd);

  window.is_fullscreen = false;
  window.has_fullscreen_restore_state = false;
  sync_window_state(state, false);
  send_window_state_changed_notification(state);
  Logger().debug("WebView window exited fullscreen");
  return {};
}

auto close_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  // ??WM_CLOSE??????
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
    case Core::WebView::State::kWM_APP_BEGIN_RESIZE: {
      if (!state || !state->webview || !state->webview->window.webview_hwnd) {
        return 0;
      }

      auto resize_edge = static_cast<WPARAM>(wparam);
      auto target_hwnd = state->webview->window.webview_hwnd;
      if (state->webview->window.is_fullscreen || IsZoomed(target_hwnd) == TRUE) {
        return 0;
      }

      ReleaseCapture();
      SendMessageW(target_hwnd, WM_SYSCOMMAND, SC_SIZE | resize_edge, 0);
      Logger().debug("WebView window entered deferred resize loop from edge code: {}", resize_edge);
      return 0;
    }

    case WM_GETMINMAXINFO: {
      MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lparam);

      // 设置最小尺寸
      mmi->ptMinTrackSize.x = 320;
      mmi->ptMinTrackSize.y = 240;
      return 0;
    }

    case WM_NCCALCSIZE: {
      // 保留标准顶层窗口语义（最大化/还原动画、Snap 等），
      // 同时移除系统默认标题栏和边框绘制，由 Web 头部承载标题栏内容。
      if (state && state->webview && !state->webview->window.is_fullscreen && wparam == TRUE) {
        auto* nc_calc_size_params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);

        if (IsZoomed(hwnd) == TRUE) {
          auto dpi = GetDpiForWindow(hwnd);
          auto insets = get_window_frame_insets_for_dpi(dpi);

          nc_calc_size_params->rgrc[0].left += insets.x;
          nc_calc_size_params->rgrc[0].right -= insets.x;
          nc_calc_size_params->rgrc[0].top += insets.y;
          nc_calc_size_params->rgrc[0].bottom -= insets.y;
        }

        return 0;
      }
      break;
    }

    case WM_DPICHANGED: {
      if (state) {
        RECT* suggested_rect = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested_rect->left, suggested_rect->top,
                     suggested_rect->right - suggested_rect->left,
                     suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        state->webview->window.x = suggested_rect->left;
        state->webview->window.y = suggested_rect->top;
      }
      return 0;
    }

    case WM_NCHITTEST: {
      if (state && Core::WebView::is_composition_active(*state)) {
        if (auto non_client_hit = Core::WebView::hit_test_non_client_region(*state, hwnd, lparam)) {
          return *non_client_hit;
        }
      }
      return HTCLIENT;
    }

    case WM_SIZE: {
      if (state) {
        sync_window_state(*state, true);

        if (wparam == SIZE_MINIMIZED) {
          break;
        }

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

    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP: {
      if (state && Core::WebView::is_composition_active(*state) &&
          Core::WebView::forward_non_client_right_button_message(*state, hwnd, msg, wparam,
                                                                 lparam)) {
        return 0;
      }
      break;
    }

    case WM_MOVE: {
      if (state) {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        state->webview->window.x = x;
        state->webview->window.y = y;
      }
      break;
    }

    case WM_ERASEBKGND: {
      if (should_paint_loading_background(state)) {
        if (auto hdc = reinterpret_cast<HDC>(wparam); hdc) {
          RECT client_rect{};
          GetClientRect(hwnd, &client_rect);
          paint_loading_background(*state, hdc, client_rect);
        }
        return 1;
      }

      if (state && Core::WebView::is_composition_active(*state)) {
        return 1;
      }
      break;
    }

    case WM_PAINT: {
      if (should_paint_loading_background(state)) {
        PAINTSTRUCT ps{};
        auto hdc = BeginPaint(hwnd, &ps);
        paint_loading_background(*state, hdc, ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
      }
      break;
    }

    case WM_SETFOCUS: {
      if (state && state->webview->resources.controller) {
        state->webview->resources.controller->MoveFocus(
            COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
      }
      break;
    }

    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK: {
      if (state && Core::WebView::is_composition_active(*state)) {
        Core::WebView::forward_mouse_message(*state, hwnd, msg, wparam, lparam);
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
  wc.hbrBackground = nullptr;
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

  // 强制让 DWM/窗口样式立即生效
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 注册窗口类
  register_window_class(state.floating_window->window.instance);

  auto style = default_window_style();
  auto ex_style = desired_window_ex_style(state);

  // 从设置读取持久化的尺寸和位置，位置居中
  int width = state.settings->raw.ui.webview_window.width;
  int height = state.settings->raw.ui.webview_window.height;
  int x = state.settings->raw.ui.webview_window.x;
  int y = state.settings->raw.ui.webview_window.y;

  // 首次启动时将默认 96 DPI 逻辑客户区尺寸缩放到当前系统 DPI，
  // 再换算为窗口外框尺寸，避免高缩放下初始窗口看起来过小。
  if (x < 0 || y < 0) {
    UINT dpi = GetDpiForSystem();
    RECT desired_client_rect = {0, 0, MulDiv(width, dpi, 96), MulDiv(height, dpi, 96)};
    if (AdjustWindowRectExForDpi(&desired_client_rect, style, FALSE, ex_style, dpi)) {
      width = desired_client_rect.right - desired_client_rect.left;
      height = desired_client_rect.bottom - desired_client_rect.top;
    } else {
      width = desired_client_rect.right;
      height = desired_client_rect.bottom;
    }
  }

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
  // - WS_SYSMENU: 保留系统菜单（Alt+Space）
  // - WS_MAXIMIZEBOX/WS_MINIMIZEBOX: 支持最大化/最小化
  HWND hwnd = CreateWindowExW(ex_style,                                // 扩展样式
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
  state.webview->window.is_visible = false;

  // 应用窗口样式（圆角 + 触发边框重算）
  apply_window_style(hwnd);

  RECT client_rect{};
  GetClientRect(hwnd, &client_rect);
  auto window_rect = get_window_rect_or_fallback(hwnd, RECT{x, y, x + width, y + height});
  state.webview->window.width = client_rect.right - client_rect.left;
  state.webview->window.height = client_rect.bottom - client_rect.top;
  state.webview->window.x = window_rect.left;
  state.webview->window.y = window_rect.top;

  Logger().info("WebView window created successfully");
  return {};
}

auto recreate_webview_host(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    return {};
  }

  apply_window_ex_style_from_settings(state);

  if (!state.webview->is_initialized) {
    Logger().info("Skipped WebView host recreation: WebView is not initialized");
    return {};
  }

  bool was_visible = IsWindowVisible(hwnd) == TRUE;

  Core::WebView::shutdown(state);
  if (auto result = Core::WebView::initialize(state, hwnd); !result) {
    return std::unexpected("Failed to recreate WebView host: " + result.error());
  }

  state.webview->window.is_visible = was_visible;
  Logger().info("WebView host recreated successfully");
  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  // 关闭 WebView
  Core::WebView::shutdown(state);

  if (state.webview->window.webview_hwnd) {
    HWND hwnd = state.webview->window.webview_hwnd;

    // Persist window bounds; when maximized, minimized, or fullscreen, save restore bounds.
    int width_to_save = state.webview->window.width;
    int height_to_save = state.webview->window.height;
    int x_to_save = state.webview->window.x;
    int y_to_save = state.webview->window.y;
    if (state.webview->window.is_fullscreen && state.webview->window.has_fullscreen_restore_state) {
      const auto& restore = state.webview->window.fullscreen_restore_placement;
      width_to_save = restore.rcNormalPosition.right - restore.rcNormalPosition.left;
      height_to_save = restore.rcNormalPosition.bottom - restore.rcNormalPosition.top;
      x_to_save = restore.rcNormalPosition.left;
      y_to_save = restore.rcNormalPosition.top;
    } else if (IsZoomed(hwnd) || IsIconic(hwnd)) {
      WINDOWPLACEMENT wp = {sizeof(wp)};
      if (GetWindowPlacement(hwnd, &wp)) {
        width_to_save = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        height_to_save = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        x_to_save = wp.rcNormalPosition.left;
        y_to_save = wp.rcNormalPosition.top;
      }
    } else {
      RECT rect = get_window_rect_or_fallback(
          hwnd, RECT{x_to_save, y_to_save, x_to_save + width_to_save, y_to_save + height_to_save});
      width_to_save = rect.right - rect.left;
      height_to_save = rect.bottom - rect.top;
      x_to_save = rect.left;
      y_to_save = rect.top;
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
    state.webview->window.is_maximized = false;
    state.webview->window.is_fullscreen = false;
    state.webview->window.has_fullscreen_restore_state = false;
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
