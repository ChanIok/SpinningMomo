module;

#include <wil/com.h>

module Features.Overlay.Capture;

import std;
import Core.State;
import Core.State.RuntimeInfo;
import Features.Overlay;
import Features.Overlay.State;
import Features.Overlay.Rendering;
import Features.Overlay.Geometry;
import Features.Overlay.Window;
import Utils.Logger;
import Utils.Graphics.Capture;
import <d3d11.h>;
import <windows.h>;

namespace Features::Overlay::Capture {

auto on_frame_arrived(Core::State::AppState& state,
                      Utils::Graphics::Capture::Direct3D11CaptureFrame frame) -> void {
  if (!state.overlay->running || !frame) {
    return;
  }

  // 冻结状态下不处理任何帧
  if (state.overlay->freeze_rendering) {
    return;
  }

  // 检查帧大小是否发生变化
  auto content_size = frame.ContentSize();
  auto& last_width = state.overlay->capture_state.last_frame_width;
  auto& last_height = state.overlay->capture_state.last_frame_height;

  bool size_changed = (content_size.Width != last_width) || (content_size.Height != last_height);

  if (size_changed) {
    // 变换流程中，只更新尺寸记录，不做其他处理
    if (state.overlay->is_transforming) {
      last_width = content_size.Width;
      last_height = content_size.Height;
      return;
    }

    // 检查是否需要退出（非变换场景，如用户手动调整窗口）
    auto [screen_width, screen_height] = Geometry::get_screen_dimensions();
    if (!Geometry::should_use_overlay(content_size.Width, content_size.Height, screen_width,
                                      screen_height)) {
      stop_overlay(state);
      return;
    }

    // 更新记录的尺寸
    last_width = content_size.Width;
    last_height = content_size.Height;

    // 重建帧池
    Utils::Graphics::Capture::recreate_frame_pool(state.overlay->capture_state.session,
                                                  content_size.Width, content_size.Height);

    state.overlay->rendering.create_new_srv = true;

    Window::set_overlay_window_size(state, content_size.Width, content_size.Height);

    // 延迟防止闪烁
    if (state.overlay->window.overlay_window_shown) {
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      Logger().debug("Capture size changed, sleeping for 400ms");
    }

    return;
  }

  auto surface = frame.Surface();
  if (!surface) {
    return;
  }

  auto texture = Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(surface);
  if (!texture) {
    return;
  }

  // 触发渲染
  Rendering::render_frame(state, texture);

  // 首次渲染时显示叠加层窗口
  if (!state.overlay->window.overlay_window_shown) {
    auto result = Window::show_overlay_window_first_time(state);
    if (!result) {
      return;
    }

    state.overlay->window.overlay_window_shown = true;

    // 首帧后自动冻结（用于窗口变换场景）
    if (state.overlay->freeze_after_first_frame) {
      state.overlay->freeze_rendering = true;
      Logger().debug("First frame rendered, overlay frozen for transform");
    }
  }
}

auto initialize_capture(Core::State::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查是否支持捕获
  if (!state.runtime_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 确保渲染系统已初始化
  auto& overlay_state = *state.overlay;
  if (!overlay_state.rendering.d3d_initialized) {
    return std::unexpected("D3D not initialized");
  }

  // 创建WinRT设备
  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(
      overlay_state.rendering.d3d_context.device.get());
  if (!winrt_device_result) {
    Logger().error("Failed to create WinRT device for capture");
    return std::unexpected("Failed to create WinRT device");
  }

  // 创建帧回调
  auto frame_callback = [&state](Utils::Graphics::Capture::Direct3D11CaptureFrame frame) {
    on_frame_arrived(state, frame);
  };

  // 创建捕获会话
  auto session_result = Utils::Graphics::Capture::create_capture_session(
      target_window, winrt_device_result.value(), width, height, frame_callback);

  if (!session_result) {
    Logger().error("Failed to create capture session");
    return std::unexpected("Failed to create capture session");
  }

  overlay_state.capture_state.session = std::move(session_result.value());

  Logger().info("Capture system initialized successfully");
  return {};
}

auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& session = state.overlay->capture_state.session;

  auto start_result = Utils::Graphics::Capture::start_capture(session);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected("Failed to start capture");
  }

  Logger().debug("Capture started successfully");
  return {};
}

auto stop_capture(Core::State::AppState& state) -> void {
  auto& session = state.overlay->capture_state.session;

  Utils::Graphics::Capture::stop_capture(session);
  Logger().debug("Capture stopped");
}

auto cleanup_capture(Core::State::AppState& state) -> void {
  auto& session = state.overlay->capture_state.session;

  Utils::Graphics::Capture::cleanup_capture_session(session);
  Logger().info("Capture resources cleaned up");
}

}  // namespace Features::Overlay::Capture
