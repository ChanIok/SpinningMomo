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
import Vendor.BuildConfig;
import Vendor.Windows;
import Vendor.ShellApi;
import Core.State.RuntimeInfo;
import Core.HttpServer.State;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

namespace UI::WebViewWindow {

// 按当前透明背景设置刷新宿主窗口扩展样式。
auto apply_window_ex_style_from_settings(Core::State::AppState& state) -> void {
  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    return;
  }

  auto current_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
  auto updated_style = current_style;
  if (state.settings->raw.ui.webview_window.enable_transparent_background) {
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

// 将最大化/全屏状态变更通知给前端标题栏。
auto send_window_state_changed_notification(Core::State::AppState& state) -> void {
  auto payload = std::format(
      R"({{"jsonrpc":"2.0","method":"window.stateChanged","params":{{"maximized":{},"fullscreen":{}}}}})",
      state.webview->window.is_maximized ? "true" : "false",
      state.webview->window.is_fullscreen ? "true" : "false");
  Core::WebView::post_message(state, payload);
}

// 同步 Win32 最大化状态，避免前端按钮状态滞后。
auto sync_window_state(Core::State::AppState& state, bool notify) -> void {
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
  return state && !state->webview->has_initial_content;
}

auto paint_loading_background(Core::State::AppState& state, HDC hdc, const RECT& rect) -> void {
  auto background = CreateSolidBrush(Core::WebView::get_loading_background_color(state));
  FillRect(hdc, &rect, background);
  DeleteObject(background);
}

// 显示已有宿主窗口；冷启动时由 WebView 导航回调触发。
auto reveal_existing_window(Core::State::AppState& state, bool activate)
    -> std::expected<void, std::string> {
  if (!state.webview->window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  auto hwnd = state.webview->window.webview_hwnd;
  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  } else {
    ShowWindow(hwnd, SW_SHOW);
  }

  if (state.webview->is_ready && state.webview->resources.controller) {
    // 隐藏窗口下创建 controller 后，显示时主动刷新可见性和 Bounds。
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    const int width = client_rect.right - client_rect.left;
    const int height = client_rect.bottom - client_rect.top;
    state.webview->window.width = width;
    state.webview->window.height = height;
    Core::WebView::resize_webview(state, width, height);
    state.webview->resources.controller->put_IsVisible(TRUE);
  }

  UpdateWindow(hwnd);
  if (activate) {
    SetForegroundWindow(hwnd);
  }
  state.webview->window.is_visible = true;

  return {};
}

auto append_hash_route(std::wstring url, std::wstring_view route) -> std::wstring {
  if (route.empty()) {
    return url;
  }

  std::wstring hash_route(route);
  if (hash_route.front() != L'#') {
    if (hash_route.front() != L'/') {
      hash_route.insert(hash_route.begin(), L'/');
    }
    hash_route = L"#" + hash_route;
  }

  if (auto hash_pos = url.find(L'#'); hash_pos != std::wstring::npos) {
    url.erase(hash_pos);
  }
  url += hash_route;
  return url;
}

auto make_webview_url(Core::State::AppState& state, std::wstring_view route) -> std::wstring {
  std::wstring url = Vendor::BuildConfig::is_debug_build()
                         ? state.webview->config.dev_server_url
                         : L"https://" + state.webview->config.virtual_host_name + L"/index.html";
  return append_hash_route(std::move(url), route);
}

auto make_browser_url(Core::State::AppState& state, std::wstring_view route) -> std::wstring {
  std::string url = std::format("http://localhost:{}/", state.http_server->port);
  return append_hash_route(std::wstring(url.begin(), url.end()), route);
}

// WebView2 不可用时退回浏览器开发入口。
auto open_in_browser(Core::State::AppState& state, std::wstring_view route) -> void {
  auto url = make_browser_url(state, route);
  Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{.cbSize = sizeof(exec_info),
                                                .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,
                                                .lpVerb = L"open",
                                                .lpFile = url.c_str(),
                                                .nShow = Vendor::ShellApi::kSW_SHOWNORMAL};
  Vendor::ShellApi::ShellExecuteExW(&exec_info);
}

// 激活主界面：冷启动延迟到导航开始后显示，热启动直接拉起窗口。
auto activate_window(Core::State::AppState& state, std::wstring_view route) -> void {
  if (state.runtime_info && !state.runtime_info->is_webview2_available) {
    Logger().warn("WebView2 runtime is unavailable. Opening in browser.");
    open_in_browser(state, route);
    return;
  }

  // 初始化尚未完成时先保存目标路由，后续初始导航会消费它。
  if (route.empty()) {
    state.webview->pending_initial_url.clear();
  } else if (!state.webview->is_ready) {
    state.webview->pending_initial_url = make_webview_url(state, route);
  }

  const bool defer_reveal_until_navigation =
      !state.webview->is_ready && !state.webview->has_initial_content;
  if (defer_reveal_until_navigation) {
    // 先让 WebView2 创建和导航推进，避免窗口过早暴露白底/黑底。
    state.webview->reveal_after_initial_navigation = [&state]() {
      if (auto result = reveal_existing_window(state, true); !result) {
        Logger().error("Failed to reveal WebView window after navigation: {}", result.error());
      }
    };
  }

  if (!state.webview->is_initialized) {
    // 创建隐藏宿主窗口并启动 WebView2 异步初始化。
    if (auto result = initialize(state); !result) {
      state.webview->reveal_after_initial_navigation = {};
      Logger().error("Failed to activate WebView window: {}", result.error());
      return;
    }
  }

  if (defer_reveal_until_navigation) {
    Logger().info("WebView window reveal deferred until initial navigation");
    return;
  }

  if (!route.empty() && state.webview->is_ready) {
    // 已有 WebView 时直接切换前端 hash 路由。
    if (auto result = Core::WebView::navigate_to_url(state, make_webview_url(state, route));
        !result) {
      Logger().warn("Failed to navigate WebView to route: {}", result.error());
    }
  }

  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    Logger().error("Failed to activate WebView window: WebView window not created");
    return;
  }

  if (auto result = reveal_existing_window(state, true); !result) {
    Logger().error("Failed to activate WebView window: {}", result.error());
    return;
  }

  Logger().info("WebView window activated");
}

// 最小化主窗口，供前端标题栏按钮调用。
auto minimize_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  ShowWindow(webview_state.window.webview_hwnd, SW_MINIMIZE);
  Logger().debug("WebView window minimized");
  return {};
}

// 在全屏状态外切换最大化，保持系统 Snap/还原行为。
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

// 切换无边框全屏，并保存还原所需的窗口样式和位置。
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
    // 先记录 WINDOWPLACEMENT，退出全屏时要回到用户原来的位置。
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

    auto fullscreen_style = window.fullscreen_restore_style &
                            ~(WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(fullscreen_style));
    SetWindowPos(hwnd, HWND_TOPMOST, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    // 前端标题栏需要知道全屏后不再把最大化当作普通窗口状态。
    window.is_fullscreen = true;
    sync_window_state(state, false);
    send_window_state_changed_notification(state);
    Logger().debug("WebView window entered fullscreen");
    return {};
  }

  if (!window.has_fullscreen_restore_state) {
    return std::unexpected("WebView fullscreen restore state is unavailable");
  }

  // 恢复样式后再恢复 placement，避免 Windows 用全屏样式解释旧尺寸。
  SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(window.fullscreen_restore_style));
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  if (!SetWindowPlacement(hwnd, &window.fullscreen_restore_placement)) {
    return std::unexpected("Failed to restore WebView window placement");
  }

  auto show_cmd = window.fullscreen_restore_placement.showCmd;
  ShowWindow(hwnd, show_cmd == SW_SHOWMINIMIZED ? SW_RESTORE : show_cmd);

  // 状态通知放在还原完成后，避免前端拿到过渡态。
  window.is_fullscreen = false;
  window.has_fullscreen_restore_state = false;
  sync_window_state(state, false);
  send_window_state_changed_notification(state);
  Logger().debug("WebView window exited fullscreen");
  return {};
}

// 走 WM_CLOSE 统一清理 WebView、保存窗口位置并销毁宿主窗口。
auto close_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.window.webview_hwnd) {
    return std::unexpected("WebView window not created");
  }

  PostMessage(webview_state.window.webview_hwnd, WM_CLOSE, 0, 0);
  Logger().debug("WebView window close requested");
  return {};
}

// 处理宿主窗口消息：转发输入、同步尺寸状态，并承接 WebView 生命周期清理。
auto window_proc(Vendor::Windows::HWND hwnd, Vendor::Windows::UINT msg,
                 Vendor::Windows::WPARAM wparam, Vendor::Windows::LPARAM lparam)
    -> Vendor::Windows::LRESULT {
  Core::State::AppState* state = nullptr;

  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    state = static_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case Core::WebView::State::kWM_APP_BEGIN_RESIZE: {
      if (!state || !state->webview->window.webview_hwnd) {
        return 0;
      }

      auto resize_edge = static_cast<WPARAM>(wparam);
      auto target_hwnd = state->webview->window.webview_hwnd;
      if (state->webview->window.is_fullscreen || IsZoomed(target_hwnd) == TRUE) {
        return 0;
      }

      // 从 WebView 事件跳回系统尺寸调整循环，保留 Win32 原生拖拽体验。
      ReleaseCapture();
      SendMessageW(target_hwnd, WM_SYSCOMMAND, SC_SIZE | resize_edge, 0);
      Logger().debug("WebView window entered deferred resize loop from edge code: {}", resize_edge);
      return 0;
    }

    // 处理虚拟主机映射协调请求，确保 WebView COM 调用在窗口线程中执行
    case Core::WebView::State::kWM_APP_RECONCILE_VIRTUAL_HOST_MAPPINGS: {
      if (!state || !state->webview->window.webview_hwnd) {
        return 0;
      }

      // WebView COM 资源必须在宿主窗口线程中协调。
      Core::WebView::reconcile_virtual_host_folder_mappings(*state);
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
      if (state && !state->webview->window.is_fullscreen && wparam == TRUE) {
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
        // 使用系统建议矩形，避免跨 DPI 显示器移动后边框尺寸异常。
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
        // 透明背景走 Composition Hosting，需要自己处理非客户区命中。
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
          // 首次导航前用主题底色占位，减少 Win32 默认白闪。
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
        // 焦点回到宿主窗口时继续交给 WebView 接管键盘输入。
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
        // 前端关闭按钮和系统关闭都走同一条持久化路径。
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

  // 同一资源同时覆盖 Alt+Tab 大图标和任务栏小图标。
  wc.hIcon = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(UI::TrayIcon::Types::IDI_ICON1), IMAGE_ICON,
                 GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
  if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

  wc.hIconSm = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(UI::TrayIcon::Types::IDI_ICON1), IMAGE_ICON,
                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  if (!wc.hIconSm) wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  RegisterClassExW(&wc);
}

// 应用 Win11 圆角，并刷新 DWM 对当前窗口样式的缓存。
auto apply_window_style(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// 创建隐藏的 WebView 宿主窗口，并恢复用户上次保存的位置和尺寸。
auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  register_window_class(state.floating_window->window.instance);

  auto style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_APPWINDOW;
  if (state.settings->raw.ui.webview_window.enable_transparent_background) {
    ex_style |= WS_EX_NOREDIRECTIONBITMAP;
  }

  int width = state.settings->raw.ui.webview_window.width;
  int height = state.settings->raw.ui.webview_window.height;
  int x = state.settings->raw.ui.webview_window.x;
  int y = state.settings->raw.ui.webview_window.y;

  if (x < 0 || y < 0) {
    // 首次启动按系统 DPI 放大默认客户区，避免高缩放下初始窗口过小。
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
    // 保存尺寸不能超过当前主显示器工作区，否则窗口可能无法拖回。
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

    // 显示器配置变化后，离屏的历史位置回退到居中。
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
  RECT window_rect{x, y, x + width, y + height};
  GetWindowRect(hwnd, &window_rect);
  state.webview->window.width = client_rect.right - client_rect.left;
  state.webview->window.height = client_rect.bottom - client_rect.top;
  state.webview->window.x = window_rect.left;
  state.webview->window.y = window_rect.top;

  Logger().info("WebView window created successfully");
  return {};
}

// 按新的宿主模式重建 WebView controller，保留当前窗口可见性。
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

  // 透明背景模式切换需要重建 controller，单纯改背景色不够。
  Core::WebView::shutdown(state);
  if (auto result = Core::WebView::initialize(state, hwnd); !result) {
    return std::unexpected("Failed to recreate WebView host: " + result.error());
  }

  state.webview->window.is_visible = was_visible;
  Logger().info("WebView host recreated successfully");
  return {};
}

// 清理 WebView 主窗口，并把可恢复的窗口位置写回设置。
auto cleanup(Core::State::AppState& state) -> void {
  // 关闭 WebView
  Core::WebView::shutdown(state);

  if (state.webview->window.webview_hwnd) {
    HWND hwnd = state.webview->window.webview_hwnd;

    int width_to_save = state.webview->window.width;
    int height_to_save = state.webview->window.height;
    int x_to_save = state.webview->window.x;
    int y_to_save = state.webview->window.y;
    if (state.webview->window.is_fullscreen && state.webview->window.has_fullscreen_restore_state) {
      // 全屏下保存进入全屏前的位置，而不是整屏尺寸。
      const auto& restore = state.webview->window.fullscreen_restore_placement;
      width_to_save = restore.rcNormalPosition.right - restore.rcNormalPosition.left;
      height_to_save = restore.rcNormalPosition.bottom - restore.rcNormalPosition.top;
      x_to_save = restore.rcNormalPosition.left;
      y_to_save = restore.rcNormalPosition.top;
    } else if (IsZoomed(hwnd) || IsIconic(hwnd)) {
      // 最大化/最小化时保存还原位置，避免下次以异常状态尺寸启动。
      WINDOWPLACEMENT wp = {sizeof(wp)};
      if (GetWindowPlacement(hwnd, &wp)) {
        width_to_save = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        height_to_save = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        x_to_save = wp.rcNormalPosition.left;
        y_to_save = wp.rcNormalPosition.top;
      }
    } else {
      RECT rect{x_to_save, y_to_save, x_to_save + width_to_save, y_to_save + height_to_save};
      GetWindowRect(hwnd, &rect);
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
      // 通过 settings 通知同步前端状态，避免关闭窗口后设置页仍拿旧尺寸。
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

// 初始化 WebView 主窗口：先创建隐藏宿主，再启动 WebView2 异步初始化。
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
