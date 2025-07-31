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

auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  auto& preview_state = *state.preview;

  // 检查是否支持捕获
  if (!state.app_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 检查预览窗口是否存在，如果不存在则初始化
  if (!preview_state.hwnd) {
    HINSTANCE instance = GetModuleHandle(nullptr);

    if (auto result = Window::initialize_preview_window(state, instance); !result) {
      return std::unexpected(result.error());
    }
  }

  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查窗口是否最小化
  if (IsIconic(target_window)) {
    return std::unexpected("Target window is minimized");
  }

  // 取消清理定时器
  if (preview_state.cleanup_timer && preview_state.cleanup_timer->IsRunning()) {
    preview_state.cleanup_timer->Cancel();
  }

  // 保存目标窗口
  preview_state.target_window = target_window;

  // 计算捕获尺寸
  RECT clientRect;
  GetClientRect(target_window, &clientRect);
  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  // 计算窗口尺寸和宽高比
  Window::calculate_window_size(preview_state, width, height);

  // 初始化渲染系统（如果需要）
  if (!preview_state.d3d_initialized) {
    auto rendering_result =
        Rendering::initialize_rendering(state, preview_state.hwnd, preview_state.size.window_width,
                                        preview_state.size.window_height);

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
  if (preview_state.is_first_show) {
    Window::handle_first_show(preview_state);
  } else {
    // 更新窗口尺寸，保持位置
    SetWindowPos(preview_state.hwnd, nullptr, 0, 0, preview_state.size.window_width,
                 preview_state.size.window_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }

  // 启动捕获
  auto start_result = Capture::start_capture(state);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected(start_result.error());
  }

  preview_state.running = true;
  preview_state.is_visible = true;

  Logger().info("Preview capture started successfully");
  return {};
}

auto stop_preview(Core::State::AppState& state) -> void {
  auto& preview_state = *state.preview;

  if (!preview_state.running) {
    return;
  }

  preview_state.running = false;
  preview_state.create_new_srv = true;

  // 停止捕获
  Capture::stop_capture(state);

  // 隐藏窗口
  Window::hide_preview_window(state);

  // 启动清理定时器
  if (!preview_state.cleanup_timer) {
    preview_state.cleanup_timer.emplace();
  }

  if (!state.preview->cleanup_timer->IsRunning()) {
    if (auto result = state.preview->cleanup_timer->SetTimer(
            std::chrono::milliseconds(3000), [&state]() { cleanup_preview(state); });
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

  // 停止预览
  stop_preview(state);

  // 清理捕获资源
  Capture::cleanup_capture(state);

  // 清理渲染资源
  Rendering::cleanup_rendering(state);

  Logger().info("Preview resources cleaned up");
}

}  // namespace Features::Preview