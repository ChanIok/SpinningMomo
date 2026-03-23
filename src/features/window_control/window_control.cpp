module;

module Features.WindowControl;

import std;
import Core.State;
import Features.Settings.State;
import Features.WindowControl.State;
import Utils.Logger;
import Utils.String;
import <windows.h>;

namespace Features::WindowControl {

auto get_virtual_screen_rect() -> RECT {
  return RECT{
      .left = GetSystemMetrics(SM_XVIRTUALSCREEN),
      .top = GetSystemMetrics(SM_YVIRTUALSCREEN),
      .right = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
      .bottom = GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN),
  };
}

auto are_rects_equal(const RECT& lhs, const RECT& rhs) -> bool {
  return lhs.left == rhs.left && lhs.top == rhs.top && lhs.right == rhs.right &&
         lhs.bottom == rhs.bottom;
}

auto is_rect_similar(const RECT& lhs, const RECT& rhs, int tolerance) -> bool {
  return std::abs(lhs.left - rhs.left) <= tolerance && std::abs(lhs.top - rhs.top) <= tolerance &&
         std::abs(lhs.right - rhs.right) <= tolerance &&
         std::abs(lhs.bottom - rhs.bottom) <= tolerance;
}

auto reset_center_lock_tracking(State::WindowControlState& window_control) -> void {
  window_control.center_lock_owned = false;
  window_control.last_center_lock_rect = RECT{};
}

auto get_clip_rect() -> std::optional<RECT> {
  RECT clip_rect{};
  if (!GetClipCursor(&clip_rect)) {
    return std::nullopt;
  }

  return clip_rect;
}

auto is_clip_cursor_active(const RECT& clip_rect) -> bool {
  return !is_rect_similar(clip_rect, get_virtual_screen_rect(), State::kClipTolerance);
}

auto get_window_title(HWND hwnd) -> std::wstring {
  int length = GetWindowTextLengthW(hwnd);
  if (length <= 0) {
    return L"";
  }

  std::wstring title(static_cast<size_t>(length) + 1, L'\0');
  int written = GetWindowTextW(hwnd, title.data(), length + 1);
  if (written <= 0) {
    return L"";
  }

  title.resize(static_cast<size_t>(written));
  return title;
}

auto get_client_rect_in_screen_coords(HWND hwnd) -> std::optional<RECT> {
  RECT client_rect{};
  if (!GetClientRect(hwnd, &client_rect)) {
    return std::nullopt;
  }

  POINT top_left{client_rect.left, client_rect.top};
  POINT bottom_right{client_rect.right, client_rect.bottom};
  if (!ClientToScreen(hwnd, &top_left) || !ClientToScreen(hwnd, &bottom_right)) {
    return std::nullopt;
  }

  return RECT{
      .left = top_left.x,
      .top = top_left.y,
      .right = bottom_right.x,
      .bottom = bottom_right.y,
  };
}

auto calculate_center_lock_rect(const RECT& client_rect) -> RECT {
  const int center_x = (client_rect.left + client_rect.right) / 2;
  const int center_y = (client_rect.top + client_rect.bottom) / 2;
  const int half_size = State::kCenterLockSize / 2;

  return RECT{
      .left = center_x - half_size,
      .top = center_y - half_size,
      .right = center_x + half_size,
      .bottom = center_y + half_size,
  };
}

auto release_center_lock_if_owned(State::WindowControlState& window_control) -> void {
  if (!window_control.center_lock_owned) {
    return;
  }

  auto current_clip_rect = get_clip_rect();
  if (current_clip_rect &&
      are_rects_equal(*current_clip_rect, window_control.last_center_lock_rect)) {
    ClipCursor(nullptr);
  }

  reset_center_lock_tracking(window_control);
}

auto process_center_lock_monitor(Core::State::AppState& state) -> void {
  if (!state.window_control || !state.settings) {
    return;
  }

  auto& window_control = *state.window_control;
  auto revert = [&]() { release_center_lock_if_owned(window_control); };

  const auto& window_settings = state.settings->raw.window;
  if (!window_settings.center_lock_cursor || window_settings.target_title.empty()) {
    revert();
    return;
  }

  HWND foreground_window = GetForegroundWindow();
  if (!foreground_window || !IsWindow(foreground_window)) {
    revert();
    return;
  }

  auto configured_title = Utils::String::FromUtf8(window_settings.target_title);
  if (configured_title.empty() || get_window_title(foreground_window) != configured_title) {
    revert();
    return;
  }

  auto clip_rect = get_clip_rect();
  if (!clip_rect || !is_clip_cursor_active(*clip_rect)) {
    revert();
    return;
  }

  auto client_rect = get_client_rect_in_screen_coords(foreground_window);
  if (!client_rect) {
    revert();
    return;
  }

  const auto center_lock_rect = calculate_center_lock_rect(*client_rect);
  if (are_rects_equal(*clip_rect, center_lock_rect)) {
    window_control.center_lock_owned = true;
    window_control.last_center_lock_rect = center_lock_rect;
    return;
  }

  if (!is_rect_similar(*clip_rect, *client_rect, State::kClipTolerance)) {
    revert();
    return;
  }

  ClipCursor(&center_lock_rect);
  window_control.center_lock_owned = true;
  window_control.last_center_lock_rect = center_lock_rect;
}

auto center_lock_monitor_thread_proc(Core::State::AppState& state, std::stop_token stop_token)
    -> void {
  auto& window_control = *state.window_control;
  std::stop_callback on_stop(
      stop_token, [&window_control]() { window_control.center_lock_monitor_cv.notify_all(); });
  std::unique_lock lock(window_control.center_lock_monitor_mutex);

  while (!stop_token.stop_requested()) {
    lock.unlock();
    process_center_lock_monitor(state);
    lock.lock();
    window_control.center_lock_monitor_cv.wait_for(lock, State::kCenterLockPollInterval);
  }

  if (state.window_control) {
    release_center_lock_if_owned(*state.window_control);
  }
}

// 查找目标窗口
auto find_target_window(const std::wstring& configured_title) -> std::expected<HWND, std::string> {
  if (configured_title.empty()) {
    return std::unexpected{"Target window not found. Please ensure the game is running."};
  }

  for (const auto& window : get_visible_windows()) {
    if (window.title == configured_title) {
      return window.handle;
    }
  }

  return std::unexpected{"Target window not found. Please ensure the game is running."};
}

// 调整窗口大小并居中
auto resize_and_center_window(HWND hwnd, int width, int height, bool activate)
    -> std::expected<void, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Failed to resize window: Invalid window handle provided."};
  }

  const int screen_w = GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = GetSystemMetrics(SM_CYSCREEN);

  // 获取窗口样式
  DWORD style = GetWindowLong(hwnd, GWL_STYLE);
  if (style == 0) {
    return std::unexpected{"Failed to get window style (GetWindowLong)."};
  }
  DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

  // 如果是有边框窗口且需要超出屏幕尺寸，转换为无边框
  if ((style & WS_OVERLAPPEDWINDOW) && (width >= screen_w || height >= screen_h)) {
    style &= ~(WS_OVERLAPPEDWINDOW);
    style |= WS_POPUP;
    if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
      return std::unexpected{"Failed to remove window border (SetWindowLong)."};
    }
  }
  // 如果是无边框窗口且高度小于屏幕高度，转换为有边框
  else if ((style & WS_POPUP) && width < screen_w && height < screen_h) {
    style &= ~(WS_POPUP);
    style |= WS_OVERLAPPEDWINDOW;
    if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
      return std::unexpected{"Failed to restore window border (SetWindowLong)."};
    }
  }

  // 调整窗口大小
  RECT rect = {0, 0, width, height};
  if (!AdjustWindowRectEx(&rect, style, FALSE, exStyle)) {
    return std::unexpected{"Failed to calculate window rectangle (AdjustWindowRectEx)."};
  }

  // 使用 rect 的 left 和 top 值来调整位置这些值通常是负数
  int totalWidth = rect.right - rect.left;
  int totalHeight = rect.bottom - rect.top;
  int borderOffsetX = rect.left;  // 左边框的偏移量（负值）
  int borderOffsetY = rect.top;   // 顶部边框的偏移量（负值）

  // 计算屏幕中心位置，考虑边框偏移
  int newLeft = (screen_w - width) / 2 + borderOffsetX;
  int newTop = (screen_h - height) / 2 + borderOffsetY;

  UINT flags = SWP_NOZORDER;
  if (!activate) {
    flags |= SWP_NOACTIVATE;
  }

  // 设置新的窗口大小和位置
  if (!SetWindowPos(hwnd, nullptr, newLeft, newTop, totalWidth, totalHeight, flags)) {
    return std::unexpected{"Failed to set window position and size (SetWindowPos)."};
  }

  // 窗口调整成功后，始终将任务栏置底
  if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr)) {
    SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  return {};
}

// 获取所有可见窗口的列表
auto get_visible_windows() -> std::vector<WindowInfo> {
  std::vector<WindowInfo> windows;

  // 回调函数
  auto enumWindowsProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
    wchar_t windowText[256];

    if (!IsWindowVisible(hwnd)) {
      return TRUE;
    }
    if (!GetWindowText(hwnd, windowText, 256)) {
      return TRUE;
    }

    auto* windows_ptr = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    if (windowText[0] != '\0') {  // 只收集有标题的窗口
      windows_ptr->push_back({hwnd, windowText});
    }

    return TRUE;
  };

  EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&windows));
  return windows;
}

// 切换窗口边框
auto toggle_window_border(HWND hwnd) -> std::expected<bool, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Failed to toggle window border: Invalid window handle provided."};
  }

  // 获取当前窗口样式
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  if (style == 0) {
    return std::unexpected{"Failed to get window style (GetWindowLong)."};
  }

  // 检查当前是否有边框
  bool hasBorder = (style & WS_OVERLAPPEDWINDOW) != 0;

  if (hasBorder) {
    // 移除边框样式
    style &= ~WS_OVERLAPPEDWINDOW;
    style |= WS_POPUP;  // 添加WS_POPUP样式
  } else {
    // 添加边框样式
    style &= ~WS_POPUP;  // 移除WS_POPUP样式
    style |= WS_OVERLAPPEDWINDOW;
  }

  // 应用新样式
  if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
    return std::unexpected{"Failed to apply new window style (SetWindowLong)."};
  }

  // 强制窗口重绘和重新布局
  if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED)) {
    return std::unexpected{"Failed to force window repaint (SetWindowPos)."};
  }

  // 返回切换后的状态
  return !hasBorder;
}

// 根据比例和总像素计算分辨率
auto calculate_resolution(double ratio, std::uint64_t total_pixels) -> Resolution {
  double height_d = std::sqrt(static_cast<double>(total_pixels) / ratio);
  double width_d = height_d * ratio;

  int height = static_cast<int>(std::round(height_d));
  int width = static_cast<int>(std::round(width_d));

  return {width, height};
}

// 根据屏幕尺寸和比例计算分辨率
auto calculate_resolution_by_screen(double ratio) -> Resolution {
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  double screen_ratio = static_cast<double>(screen_width) / screen_height;

  if (ratio > screen_ratio) {
    // 宽比例，以屏幕宽度为基准
    int width = screen_width;
    int height = static_cast<int>(std::round(width / ratio));
    return {width, height};
  } else {
    // 高比例，以屏幕高度为基准
    int height = screen_height;
    int width = static_cast<int>(std::round(height * ratio));
    return {width, height};
  }
}

// 应用窗口变换
auto apply_window_transform(HWND target_window, const Resolution& resolution,
                            const TransformOptions& options) -> std::expected<void, std::string> {
  auto result = resize_and_center_window(target_window, resolution.width, resolution.height,
                                         options.activate_window);
  if (!result) {
    return std::unexpected{result.error()};
  }
  return {};
}

// 重置窗口到屏幕尺寸
auto reset_window_to_screen(HWND target_window, const TransformOptions& options)
    -> std::expected<void, std::string> {
  const int screen_width = GetSystemMetrics(SM_CXSCREEN);
  const int screen_height = GetSystemMetrics(SM_CYSCREEN);
  return apply_window_transform(target_window, Resolution{screen_width, screen_height}, options);
}

auto start_center_lock_monitor(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.window_control) {
    return std::unexpected{"Window control state is not initialized."};
  }

  auto& window_control = *state.window_control;
  if (window_control.center_lock_monitor_thread.joinable()) {
    return {};
  }

  try {
    window_control.center_lock_monitor_thread = std::jthread([&state](std::stop_token stop_token) {
      center_lock_monitor_thread_proc(state, stop_token);
    });
    Logger().info("Window control center lock monitor started");
    return {};
  } catch (const std::exception& e) {
    return std::unexpected{"Failed to start center lock monitor: " + std::string(e.what())};
  }
}

auto stop_center_lock_monitor(Core::State::AppState& state) -> void {
  if (!state.window_control) {
    return;
  }

  auto& window_control = *state.window_control;
  if (window_control.center_lock_monitor_thread.joinable()) {
    window_control.center_lock_monitor_thread.request_stop();
    window_control.center_lock_monitor_cv.notify_all();
    window_control.center_lock_monitor_thread.join();
  } else {
    release_center_lock_if_owned(window_control);
  }

  Logger().info("Window control center lock monitor stopped");
}

}  // namespace Features::WindowControl
