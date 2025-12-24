module;

module Features.Preview.Window;

import std;
import Core.State;
import Core.State.AppInfo;
import Features.Preview.State;
import Features.Preview.Types;
import Features.Preview.Interaction;
import Features.Preview.Rendering;
import Features.Preview.Capture;
import Utils.Graphics.D3D;
import Utils.Graphics.Capture;
import Utils.Timer;
import Utils.Logger;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

namespace Features::Preview::Window {

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
  auto [handled, lresult] =
      Features::Preview::Interaction::handle_preview_message(*state, hwnd, message, wParam, lParam);

  if (handled) {
    return lresult;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

auto register_preview_window_class(HINSTANCE instance) -> bool {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = preview_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = Features::Preview::Types::PREVIEW_WINDOW_CLASS;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.style = CS_HREDRAW | CS_VREDRAW;

  return RegisterClassExW(&wc) != 0;
}

auto create_preview_window(HINSTANCE instance, int width, int height, Core::State::AppState* state)
    -> HWND {
  return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
                         Features::Preview::Types::PREVIEW_WINDOW_CLASS, L"PreviewWindow", WS_POPUP,
                         0, 0, width, height, nullptr, nullptr, instance, state);
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

auto set_preview_window_size(Features::Preview::State::PreviewState& state, int capture_width,
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

  if (state.is_first_show) {
    state.is_first_show = false;
    SetWindowPos(state.hwnd, nullptr, 20, 20, state.size.window_width, state.size.window_height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  } else {
    SetWindowPos(state.hwnd, nullptr, 0, 0, state.size.window_width, state.size.window_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

auto create_window(HINSTANCE instance, Core::State::AppState& state)
    -> std::expected<HWND, std::string> {
  // 1. 注册窗口类
  if (!register_preview_window_class(instance)) {
    return std::unexpected("Failed to register preview window class");
  }

  int screenHeight = GetSystemMetrics(SM_CYSCREEN);
  int idealSize = static_cast<int>(screenHeight * 0.5);

  // 2. 创建窗口
  HWND hwnd = create_preview_window(instance, idealSize, idealSize + 24, &state);
  if (!hwnd) {
    return std::unexpected("Failed to create preview window");
  }

  // 3. 设置窗口外观
  setup_window_appearance(hwnd);
  return hwnd;
}

auto initialize_preview_window(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string> {
  // 创建窗口，传递状态引用
  auto window_result = create_window(instance, state);
  if (!window_result) {
    return std::unexpected(window_result.error());
  }

  // 初始化预览状态
  state.preview->hwnd = window_result.value();
  state.preview->is_first_show = true;

  // 初始化DPI
  HDC hdc = GetDC(nullptr);
  UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  ReleaseDC(nullptr, hdc);
  state.preview->dpi_sizes.update_dpi_scaling(dpi);

  // 计算理想尺寸范围
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);
  state.preview->size.min_ideal_size = std::min(screenWidth, screenHeight) / 10;
  state.preview->size.max_ideal_size = std::max(screenWidth, screenHeight);
  state.preview->size.ideal_size = screenHeight / 2;

  return {};
}

auto show_preview_window(Core::State::AppState& state) -> void {
  if (state.preview->hwnd) {
    ShowWindow(state.preview->hwnd, SW_SHOW);
  }
}

auto hide_preview_window(Core::State::AppState& state) -> void {
  if (state.preview->hwnd) {
    ShowWindow(state.preview->hwnd, SW_HIDE);
  }
}

auto update_preview_window_dpi(Core::State::AppState& state, UINT new_dpi) -> void {
  if (state.preview->hwnd) {
    // 获取当前窗口位置和大小
    RECT rect;
    GetWindowRect(state.preview->hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 更新窗口
    SetWindowPos(state.preview->hwnd, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // 强制重绘
    InvalidateRect(state.preview->hwnd, nullptr, TRUE);
  }
}

auto destroy_preview_window(Core::State::AppState& state) -> void {
  if (state.preview->hwnd) {
    DestroyWindow(state.preview->hwnd);
    state.preview->hwnd = nullptr;
  }
}

}  // namespace Features::Preview::Window