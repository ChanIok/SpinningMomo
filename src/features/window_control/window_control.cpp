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

auto reset_layered_capture_workaround_tracking(State::WindowControlState& window_control) -> void {
  window_control.layered_capture_workaround_owned = false;
  window_control.layered_capture_workaround_hwnd = nullptr;
}

// GetWindowLong：返回值为 0 时可能是合法样式，需用 LastError 区分失败。
auto get_window_long_checked(HWND hwnd, int index, const char* field_label)
    -> std::expected<LONG, std::string> {
  SetLastError(0);
  const LONG value = GetWindowLong(hwnd, index);
  if (value == 0 && GetLastError() != 0) {
    return std::unexpected{std::format("Failed to get window {} (GetWindowLong).", field_label)};
  }
  return value;
}

// SetWindowLong：成功时返回先前值，先前值也可能为 0。
auto set_window_long_checked(HWND hwnd, int index, LONG new_value, std::string error_message)
    -> std::expected<void, std::string> {
  SetLastError(0);
  if (SetWindowLong(hwnd, index, new_value) == 0 && GetLastError() != 0) {
    return std::unexpected{std::move(error_message)};
  }
  return {};
}

auto set_layered_window_style(HWND hwnd, DWORD ex_style, bool enabled)
    -> std::expected<DWORD, std::string> {
  DWORD next_style = enabled ? (ex_style | WS_EX_LAYERED) : (ex_style & ~WS_EX_LAYERED);
  if (next_style == ex_style) {
    return ex_style;
  }

  SetLastError(0);
  if (SetWindowLong(hwnd, GWL_EXSTYLE, next_style) == 0 && GetLastError() != 0) {
    return std::unexpected{enabled ? "Failed to enable layered window style (SetWindowLong)."
                                   : "Failed to disable layered window style (SetWindowLong)."};
  }

  return next_style;
}

auto release_layered_capture_workaround_if_owned(State::WindowControlState& window_control,
                                                 HWND hwnd) -> std::expected<void, std::string> {
  if (!window_control.layered_capture_workaround_owned ||
      window_control.layered_capture_workaround_hwnd != hwnd) {
    return {};
  }

  if (!hwnd || !IsWindow(hwnd)) {
    reset_layered_capture_workaround_tracking(window_control);
    return {};
  }

  auto ex_style_long = get_window_long_checked(hwnd, GWL_EXSTYLE, "ex style");
  if (!ex_style_long) {
    return std::unexpected{ex_style_long.error()};
  }
  const DWORD ex_style = static_cast<DWORD>(*ex_style_long);

  auto release_result = set_layered_window_style(hwnd, ex_style, false);
  if (!release_result) {
    return std::unexpected{release_result.error()};
  }

  reset_layered_capture_workaround_tracking(window_control);
  return {};
}

auto apply_layered_capture_workaround(Core::State::AppState& state, HWND hwnd, int width,
                                      int height, DWORD ex_style)
    -> std::expected<DWORD, std::string> {
  auto* window_control = state.window_control.get();
  if (!window_control) {
    return ex_style;
  }

  const int screen_w = GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = GetSystemMetrics(SM_CYSCREEN);
  const bool oversized = width > screen_w || height > screen_h;
  const bool workaround_enabled =
      state.settings && state.settings->raw.window.enable_layered_capture_workaround;

  if (window_control->layered_capture_workaround_owned &&
      window_control->layered_capture_workaround_hwnd != hwnd) {
    if (auto release_result = release_layered_capture_workaround_if_owned(
            *window_control, window_control->layered_capture_workaround_hwnd);
        !release_result) {
      return std::unexpected{release_result.error()};
    }
  }

  if (workaround_enabled && oversized) {
    const bool already_layered = (ex_style & WS_EX_LAYERED) != 0;
    auto apply_result = set_layered_window_style(hwnd, ex_style, true);
    if (!apply_result) {
      return std::unexpected{apply_result.error()};
    }

    if (!already_layered) {
      window_control->layered_capture_workaround_owned = true;
      window_control->layered_capture_workaround_hwnd = hwnd;
    } else if (!window_control->layered_capture_workaround_owned ||
               window_control->layered_capture_workaround_hwnd != hwnd) {
      reset_layered_capture_workaround_tracking(*window_control);
    }

    return apply_result.value();
  }

  if (auto release_result = release_layered_capture_workaround_if_owned(*window_control, hwnd);
      !release_result) {
    return std::unexpected{release_result.error()};
  }

  auto refreshed = get_window_long_checked(hwnd, GWL_EXSTYLE, "ex style");
  if (!refreshed) {
    return std::unexpected{refreshed.error()};
  }
  return static_cast<DWORD>(*refreshed);
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

auto get_window_process_id(HWND hwnd) -> DWORD {
  DWORD process_id = 0;
  GetWindowThreadProcessId(hwnd, &process_id);
  return process_id;
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

  // 只有当前 ClipCursor 仍然等于我们上次设置的小矩形时才释放。
  // 如果游戏已经重新写入了新的 clip 区域，这里应当让游戏继续接管。
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
  // 任一前置条件不满足时，都回到“若是我们接管的 clip，则安全释放”的统一收口。
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
  // 这里先根据设置解析目标 HWND，再比较 foreground HWND。
  // 不再直接读取前台窗口标题，避免退出阶段对本进程窗口发送同步文本消息而死锁。
  auto target_window = find_target_window(configured_title);
  if (!target_window || foreground_window != *target_window) {
    revert();
    return;
  }

  auto clip_rect = get_clip_rect();
  if (!clip_rect || !is_clip_cursor_active(*clip_rect)) {
    revert();
    return;
  }

  auto client_rect = get_client_rect_in_screen_coords(*target_window);
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
  // stop_token 触发时立即唤醒 wait_for，使 shutdown 阶段的 join() 可以快速返回。
  std::stop_callback on_stop(
      stop_token, [&window_control]() { window_control.center_lock_monitor_cv.notify_all(); });
  std::unique_lock lock(window_control.center_lock_monitor_mutex);

  while (!stop_token.stop_requested()) {
    // 监控逻辑不需要持有互斥量；这里只把锁用于可中断等待。
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
auto resize_and_center_window(Core::State::AppState& state, HWND hwnd, int width, int height,
                              bool activate) -> std::expected<void, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Failed to resize window: Invalid window handle provided."};
  }

  const int screen_w = GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = GetSystemMetrics(SM_CYSCREEN);

  auto style_long = get_window_long_checked(hwnd, GWL_STYLE, "style");
  if (!style_long) {
    return std::unexpected{style_long.error()};
  }
  DWORD style = static_cast<DWORD>(*style_long);

  auto ex_style_long = get_window_long_checked(hwnd, GWL_EXSTYLE, "ex style");
  if (!ex_style_long) {
    return std::unexpected{ex_style_long.error()};
  }
  DWORD exStyle = static_cast<DWORD>(*ex_style_long);

  auto ex_style_result = apply_layered_capture_workaround(state, hwnd, width, height, exStyle);
  if (!ex_style_result) {
    return std::unexpected{ex_style_result.error()};
  }
  exStyle = ex_style_result.value();

  // 如果是有边框窗口且需要超出屏幕尺寸，转换为无边框
  if ((style & WS_OVERLAPPEDWINDOW) && (width >= screen_w || height >= screen_h)) {
    style &= ~(WS_OVERLAPPEDWINDOW);
    style |= WS_POPUP;
    if (auto r = set_window_long_checked(hwnd, GWL_STYLE, static_cast<LONG>(style),
                                         "Failed to remove window border (SetWindowLong).");
        !r) {
      return std::unexpected{r.error()};
    }
  }
  // 如果是无边框窗口且高度小于屏幕高度，转换为有边框
  else if ((style & WS_POPUP) && width < screen_w && height < screen_h) {
    style &= ~(WS_POPUP);
    style |= WS_OVERLAPPEDWINDOW;
    if (auto r = set_window_long_checked(hwnd, GWL_STYLE, static_cast<LONG>(style),
                                         "Failed to restore window border (SetWindowLong).");
        !r) {
      return std::unexpected{r.error()};
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
    // 跳过本进程窗口，避免锁鼠监控把浮窗 / WebView / 菜单等误当作候选目标，
    // 也顺带规避本进程窗口文本读取带来的同步消息风险。
    if (get_window_process_id(hwnd) == GetCurrentProcessId()) {
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

  auto style_long = get_window_long_checked(hwnd, GWL_STYLE, "style");
  if (!style_long) {
    return std::unexpected{style_long.error()};
  }
  LONG style = *style_long;

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

  if (auto r = set_window_long_checked(hwnd, GWL_STYLE, style,
                                       "Failed to apply new window style (SetWindowLong).");
      !r) {
    return std::unexpected{r.error()};
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
auto apply_window_transform(Core::State::AppState& state, HWND target_window,
                            const Resolution& resolution, const TransformOptions& options)
    -> std::expected<void, std::string> {
  auto result = resize_and_center_window(state, target_window, resolution.width, resolution.height,
                                         options.activate_window);
  if (!result) {
    return std::unexpected{result.error()};
  }
  return {};
}

// 重置窗口到屏幕尺寸
auto reset_window_to_screen(Core::State::AppState& state, HWND target_window,
                            const TransformOptions& options) -> std::expected<void, std::string> {
  const int screen_width = GetSystemMetrics(SM_CXSCREEN);
  const int screen_height = GetSystemMetrics(SM_CYSCREEN);
  return apply_window_transform(state, target_window, Resolution{screen_width, screen_height},
                                options);
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

  if (auto release_result = release_layered_capture_workaround_if_owned(
          window_control, window_control.layered_capture_workaround_hwnd);
      !release_result) {
    Logger().warn("Failed to release layered capture workaround on shutdown: {}",
                  release_result.error());
  }

  Logger().info("Window control center lock monitor stopped");
}

}  // namespace Features::WindowControl
