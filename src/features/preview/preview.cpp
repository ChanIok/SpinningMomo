module;

module Features.Preview;

import std;
import Core.State;
import Core.State.RuntimeInfo;
import Features.Preview.State;
import Features.Preview.Types;
import Features.Preview.Window;
import Features.Preview.Interaction;
import Features.Preview.Rendering;
import Features.Preview.Capture;
import Utils.Graphics.D3D;
import Utils.Graphics.Capture;
import Utils.Logger;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

namespace Features::Preview {

auto send_preview_control_message(HWND preview_hwnd, UINT message) -> bool {
  if (!preview_hwnd || !IsWindow(preview_hwnd)) {
    return false;
  }

  return SendMessageW(preview_hwnd, message, 0, 0) != 0;
}

auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  auto& preview_state = *state.preview;

  // 检查是否支持捕获
  if (!state.runtime_info->is_capture_supported) {
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

  if (!send_preview_control_message(preview_state.hwnd, Types::WM_CANCEL_PREVIEW_CLEANUP)) {
    Logger().warn("Failed to cancel pending preview cleanup");
  }

  // 保存目标窗口
  preview_state.target_window = target_window;

  // 计算捕获尺寸
  RECT clientRect;
  GetClientRect(target_window, &clientRect);
  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  // 初始化渲染系统（如果需要）
  if (!preview_state.rendering_resources.initialized.load(std::memory_order_acquire)) {
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
  // 计算窗口尺寸和宽高比
  Window::set_preview_window_size(preview_state, width, height);

  Window::show_preview_window(state);

  // 启动捕获
  auto start_result = Capture::start_capture(state);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected(start_result.error());
  }

  preview_state.running.store(true, std::memory_order_release);

  Logger().info("Preview capture started successfully");
  return {};
}

auto stop_preview(Core::State::AppState& state) -> void {
  auto& preview_state = *state.preview;

  if (!preview_state.running.load(std::memory_order_acquire)) {
    return;
  }

  preview_state.running.store(false, std::memory_order_release);
  preview_state.create_new_srv.store(true, std::memory_order_release);

  // 停止捕获
  Capture::stop_capture(state);

  // 隐藏窗口
  Window::hide_preview_window(state);

  if (!send_preview_control_message(preview_state.hwnd, Types::WM_SCHEDULE_PREVIEW_CLEANUP)) {
    cleanup_preview(state);
  }

  Logger().info("Preview capture stopped");
}

auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void {
  state.preview->dpi_sizes.update_dpi_scaling(new_dpi);
  Window::update_preview_window_dpi(state, new_dpi);
}

auto cleanup_preview(Core::State::AppState& state) -> void {
  if (send_preview_control_message(state.preview->hwnd, Types::WM_IMMEDIATE_PREVIEW_CLEANUP)) {
    return;
  }

  Capture::cleanup_capture(state);
  Rendering::cleanup_rendering(state);

  Logger().info("Preview resources cleaned up");
}

}  // namespace Features::Preview
