module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <functional>
#include <iostream>

module Features.Preview.Window;

import std;
import Types.Preview;
import Core.State;
import Utils.Graphics.D3D;
import Utils.Graphics.Capture;
import Utils.Timer;
import Utils.Logger;
import Core.Constants;
import Features.Preview.Interaction;
import Features.Preview.Rendering;
import Features.Preview.CaptureIntegration;

namespace Features::Preview::Window {

// 窗口类名
constexpr wchar_t PREVIEW_WINDOW_CLASS[] = L"SpinningMomoPreviewWindowClass";

// 前向声明
LRESULT CALLBACK preview_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
auto register_preview_window_class(HINSTANCE instance) -> bool;
auto create_preview_window(HINSTANCE instance, HWND parent, int width, int height,
                           Core::State::AppState* state) -> HWND;
auto setup_window_appearance(HWND hwnd) -> void;
auto calculate_window_size(Types::Preview::PreviewState& state, int capture_width,
                           int capture_height) -> void;
auto handle_first_show(Types::Preview::PreviewState& state) -> void;

auto create_window(HINSTANCE instance, HWND parent, Core::State::AppState* state)
    -> std::expected<HWND, std::string> {
  // 1. 注册窗口类
  if (!register_preview_window_class(instance)) {
    return std::unexpected("Failed to register preview window class");
  }

  int screenHeight = GetSystemMetrics(SM_CYSCREEN);
  int idealSize = static_cast<int>(screenHeight * 0.5);

  // 2. 创建窗口
  HWND hwnd = create_preview_window(instance, parent, idealSize, idealSize + 24, state);
  if (!hwnd) {
    return std::unexpected("Failed to create preview window");
  }

  // 3. 设置窗口外观
  setup_window_appearance(hwnd);
  return hwnd;
}

auto initialize_preview(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string> {
  // 创建窗口，传递状态指针
  auto window_result = create_window(instance, parent, &state);
  if (!window_result) {
    return std::unexpected(window_result.error());
  }

  // 初始化预览状态
  state.preview.hwnd = window_result.value();
  state.preview.main_window = parent;
  state.preview.is_first_show = true;

  // 初始化DPI
  HDC hdc = GetDC(nullptr);
  UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  ReleaseDC(nullptr, hdc);
  state.preview.dpi_sizes.update_dpi_scaling(dpi);

  // 计算理想尺寸范围
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);
  state.preview.size.min_ideal_size = std::min(screenWidth, screenHeight) / 10;
  state.preview.size.max_ideal_size = std::max(screenWidth, screenHeight);
  state.preview.size.ideal_size = screenHeight / 2;

  Logger().info("Preview window initialized successfully");
  return {};
}

auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查窗口是否最小化
  if (IsIconic(target_window)) {
    Logger().debug("Target window is minimized, cannot start capture");
    return std::unexpected("Target window is minimized");
  }

  // 取消清理定时器
  if (state.preview.cleanup_timer && state.preview.cleanup_timer->IsRunning()) {
    state.preview.cleanup_timer->Cancel();
  }

  // 保存目标窗口
  state.preview.target_window = target_window;

  // 计算捕获尺寸
  RECT clientRect;
  GetClientRect(target_window, &clientRect);
  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  // 计算窗口尺寸和宽高比
  calculate_window_size(state.preview, width, height);

  // 检查是否支持捕获
  if (!Utils::Graphics::Capture::is_capture_supported()) {
    return std::unexpected("Capture not supported on this system");
  }

  // 初始化渲染系统（如果需要）
  if (!state.preview.d3d_initialized) {
    auto rendering_result = Features::Preview::Rendering::initialize_rendering(
        state, state.preview.hwnd, state.preview.size.window_width,
        state.preview.size.window_height);

    if (!rendering_result) {
      Logger().error("Failed to initialize rendering system");
      return std::unexpected(rendering_result.error());
    }
  }

  // 初始化捕获系统
  auto capture_result = Features::Preview::CaptureIntegration::initialize_capture(
      state, target_window, width, height);

  if (!capture_result) {
    Logger().error("Failed to initialize capture system");
    return std::unexpected(capture_result.error());
  }

  // 处理首次显示
  if (state.preview.is_first_show) {
    handle_first_show(state.preview);
  } else {
    // 更新窗口尺寸，保持位置
    SetWindowPos(state.preview.hwnd, nullptr, 0, 0, state.preview.size.window_width,
                 state.preview.size.window_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }

  // 启动捕获
  auto start_result = Features::Preview::CaptureIntegration::start_capture(state);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected(start_result.error());
  }

  state.preview.running = true;
  state.preview.is_visible = true;

  Logger().info("Preview capture started successfully");
  return {};
}

auto stop_preview(Core::State::AppState& state) -> void {
  if (!state.preview.running) {
    return;
  }

  state.preview.running = false;
  state.preview.create_new_srv = true;

  // 停止捕获
  Features::Preview::CaptureIntegration::stop_capture(state);

  // 隐藏窗口
  hide_preview_window(state);

  // 启动清理定时器
  if (!state.preview.cleanup_timer) {
    state.preview.cleanup_timer.emplace();
  }

  if (!state.preview.cleanup_timer->IsRunning()) {
    if (auto result = state.preview.cleanup_timer->SetTimer(std::chrono::milliseconds(30000),
                                                            [&state]() { cleanup_preview(state); });
        !result) {
      Logger().error("Failed to set cleanup timer: {}", static_cast<int>(result.error()));
    }
  }

  Logger().info("Preview capture stopped");
}

auto show_preview_window(Core::State::AppState& state) -> void {
  if (state.preview.hwnd) {
    ShowWindow(state.preview.hwnd, SW_SHOWNA);
    UpdateWindow(state.preview.hwnd);
    state.preview.is_visible = true;
  }
}

auto hide_preview_window(Core::State::AppState& state) -> void {
  if (state.preview.hwnd) {
    ShowWindow(state.preview.hwnd, SW_HIDE);
    state.preview.is_visible = false;
  }
}

auto is_preview_window_visible(const Core::State::AppState& state) -> bool {
  return state.preview.hwnd && IsWindowVisible(state.preview.hwnd);
}

auto resize_preview_window(Core::State::AppState& state, int width, int height) -> void {
  if (state.preview.hwnd) {
    SetWindowPos(state.preview.hwnd, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    state.preview.size.window_width = width;
    state.preview.size.window_height = height;
  }
}

auto move_preview_window(Core::State::AppState& state, int x, int y) -> void {
  if (state.preview.hwnd) {
    SetWindowPos(state.preview.hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void {
  state.preview.dpi_sizes.update_dpi_scaling(new_dpi);

  if (state.preview.hwnd) {
    // 获取当前窗口位置和大小
    RECT rect;
    GetWindowRect(state.preview.hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 更新窗口
    SetWindowPos(state.preview.hwnd, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // 强制重绘
    InvalidateRect(state.preview.hwnd, nullptr, TRUE);
  }
}

auto cleanup_preview(Core::State::AppState& state) -> void {
  if (state.preview.cleanup_timer) {
    state.preview.cleanup_timer->Cancel();
    state.preview.cleanup_timer.reset();
  }

  // 清理捕获资源
  Features::Preview::CaptureIntegration::cleanup_capture(state);

  // 清理渲染资源
  Features::Preview::Rendering::cleanup_rendering(state);

  state.preview.d3d_initialized = false;
  state.preview.running = false;
  state.preview.target_window = nullptr;
  state.preview.create_new_srv = true;

  Logger().info("Preview resources cleaned up");
}

auto get_preview_window_handle(const Core::State::AppState& state) -> HWND {
  return state.preview.hwnd;
}

auto is_preview_running(const Core::State::AppState& state) -> bool {
  return state.preview.running;
}

// 内部实现函数

auto register_preview_window_class(HINSTANCE instance) -> bool {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = preview_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = PREVIEW_WINDOW_CLASS;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.style = CS_HREDRAW | CS_VREDRAW;

  return RegisterClassExW(&wc) != 0;
}

auto create_preview_window(HINSTANCE instance, HWND parent, int width, int height,
                           Core::State::AppState* state) -> HWND {
  return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, PREVIEW_WINDOW_CLASS,
                         L"PreviewWindow", WS_POPUP, 0, 0, width, height, nullptr, nullptr,
                         instance, state);
}

auto setup_window_appearance(HWND hwnd) -> void {
  // 设置透明度
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // 设置DWM属性
  MARGINS margins = {1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);

  DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
  DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));

  BOOL value = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));

  // Windows 11 圆角
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

auto calculate_window_size(Types::Preview::PreviewState& state, int capture_width,
                           int capture_height) -> void {
  state.size.aspect_ratio = static_cast<float>(capture_height) / capture_width;

  if (state.size.aspect_ratio >= 1.0f) {
    // 高度大于等于宽度
    state.size.window_height = state.size.ideal_size;
    state.size.window_width = static_cast<int>(state.size.window_height / state.size.aspect_ratio);
  } else {
    // 宽度大于高度
    state.size.window_width = state.size.ideal_size;
    state.size.window_height = static_cast<int>(state.size.window_width * state.size.aspect_ratio);
  }
}

auto handle_first_show(Types::Preview::PreviewState& state) -> void {
  state.is_first_show = false;
  int x = 20;  // 默认位置
  int y = 20;

  SetWindowPos(state.hwnd, nullptr, x, y, state.size.window_width, state.size.window_height,
               SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

// 窗口过程 - 使用GWLP_USERDATA模式获取状态
LRESULT CALLBACK preview_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Core::State::AppState* state = nullptr;

  if (message == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    state = reinterpret_cast<Core::State::AppState*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  } else {
    state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (!state) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  // 使用交互模块处理消息
  auto [result, lresult] =
      Features::Preview::Interaction::handle_preview_message(*state, hwnd, message, wParam, lParam);

  switch (result) {
    case Features::Preview::Interaction::MessageResult::Handled:
      return lresult;
    case Features::Preview::Interaction::MessageResult::NotHandled:
      // 继续处理其他消息
      break;
    case Features::Preview::Interaction::MessageResult::Default:
      return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace Features::Preview::Window