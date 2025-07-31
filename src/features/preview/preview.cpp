module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <functional>
#include <iostream>

module Features.Preview;

import std;
import Core.State;
import Core.State.AppInfo;
import Features.Preview.State;
import Features.Preview.Types;
import Features.Preview.Window;
import Features.Preview.Interaction;
import Features.Preview.Rendering;
import Features.Preview.Capture;
import Utils.Graphics.D3D;
import Utils.Graphics.Capture;
import Utils.Timer;
import Utils.Logger;

namespace Features::Preview {

auto initialize_preview(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string> {
  // 创建窗口，传递状态指针
  auto window_result = Window::create_window(instance, &state);
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

  Logger().info("Preview window initialized successfully");
  return {};
}

auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  // 检查预览窗口是否存在，如果不存在则初始化
  if (!state.preview->hwnd) {
    HINSTANCE instance = GetModuleHandle(nullptr);
    Logger().debug("Initializing preview window");
    if (auto result = initialize_preview(state, instance); !result) {
      return std::unexpected(result.error());
    }
  }

  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查窗口是否最小化
  if (IsIconic(target_window)) {
    Logger().debug("Target window is minimized, cannot start capture");
    return std::unexpected("Target window is minimized");
  }

  // 取消清理定时器
  if (state.preview->cleanup_timer && state.preview->cleanup_timer->IsRunning()) {
    state.preview->cleanup_timer->Cancel();
  }

  // 保存目标窗口
  state.preview->target_window = target_window;

  // 计算捕获尺寸
  RECT clientRect;
  GetClientRect(target_window, &clientRect);
  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  // 计算窗口尺寸和宽高比
  Window::calculate_window_size(*state.preview, width, height);

  // 检查是否支持捕获
  if (!state.app_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 初始化渲染系统（如果需要）
  if (!state.preview->d3d_initialized) {
    auto rendering_result = Rendering::initialize_rendering(state, state.preview->hwnd,
                                                            state.preview->size.window_width,
                                                            state.preview->size.window_height);

    if (!rendering_result) {
      Logger().error("Failed to initialize rendering system");
      return std::unexpected(rendering_result.error());
    }
  }

  // 初始化捕获系统
  auto capture_result = Capture::initialize_capture(state, target_window, width, height);

  if (!capture_result) {
    Logger().error("Failed to initialize capture system");
    return std::unexpected(capture_result.error());
  }

  // 处理首次显示
  if (state.preview->is_first_show) {
    Window::handle_first_show(*state.preview);
  } else {
    // 更新窗口尺寸，保持位置
    SetWindowPos(state.preview->hwnd, nullptr, 0, 0, state.preview->size.window_width,
                 state.preview->size.window_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }

  // 启动捕获
  auto start_result = Capture::start_capture(state);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected(start_result.error());
  }

  state.preview->running = true;
  state.preview->is_visible = true;

  Logger().info("Preview capture started successfully");
  return {};
}

auto stop_preview(Core::State::AppState& state) -> void {
  if (!state.preview->running) {
    return;
  }

  state.preview->running = false;
  state.preview->create_new_srv = true;

  // 停止捕获
  Capture::stop_capture(state);

  // 隐藏窗口
  Window::hide_preview_window(state);

  // 启动清理定时器
  if (!state.preview->cleanup_timer) {
    state.preview->cleanup_timer.emplace();
  }

  if (!state.preview->cleanup_timer->IsRunning()) {
    if (auto result = state.preview->cleanup_timer->SetTimer(
            std::chrono::milliseconds(30000), [&state]() { cleanup_preview(state); });
        !result) {
      Logger().error("Failed to set cleanup timer: {}", static_cast<int>(result.error()));
    }
  }

  Logger().info("Preview capture stopped");
}

auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void {
  state.preview->dpi_sizes.update_dpi_scaling(new_dpi);
  Window::update_preview_window_dpi(state, new_dpi);
}

auto cleanup_preview(Core::State::AppState& state) -> void {
  if (state.preview->cleanup_timer) {
    state.preview->cleanup_timer->Cancel();
    state.preview->cleanup_timer.reset();
  }

  // 清理捕获资源
  Capture::cleanup_capture(state);

  // 清理渲染资源
  Rendering::cleanup_rendering(state);

  state.preview->d3d_initialized = false;
  state.preview->running = false;
  state.preview->target_window = nullptr;
  state.preview->create_new_srv = true;

  Logger().info("Preview resources cleaned up");
}

}  // namespace Features::Preview