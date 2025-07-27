module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <functional>
#include <iostream>

module Features.Overlay.Capture;

import std;
import Core.State;
import Core.State.AppInfo;
import Features.Overlay.State;
import Features.Overlay.Rendering;
import Features.Overlay.Utils;
import Features.Overlay.Window;
import Utils.Logger;
import Utils.Graphics.Capture;

namespace Features::Overlay::Capture {

auto on_frame_arrived(Core::State::AppState& state,
                      Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void {
  if (!state.overlay->running || !frame_texture) {
    return;
  }

  // 触发渲染
  Rendering::render_frame(state, frame_texture);
}

auto initialize_capture(Core::State::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查是否支持捕获
  if (!state.app_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 确保渲染系统已初始化
  auto& overlay_state = *state.overlay;
  if (!overlay_state.rendering.d3d_initialized) {
    return std::unexpected("D3D not initialized");
  }

  // 创建WinRT设备
  auto winrt_device_result = ::Utils::Graphics::Capture::create_winrt_device(
      overlay_state.rendering.d3d_context.device.Get());
  if (!winrt_device_result) {
    Logger().error("Failed to create WinRT device for capture");
    return std::unexpected("Failed to create WinRT device");
  }

  // 创建帧回调
  auto frame_callback = [&state](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
    on_frame_arrived(state, texture);
  };

  // 创建捕获会话
  auto session_result = ::Utils::Graphics::Capture::create_capture_session(
      target_window, winrt_device_result.value(), width, height, frame_callback);

  if (!session_result) {
    Logger().error("Failed to create capture session");
    return std::unexpected("Failed to create capture session");
  }

  overlay_state.capture_state.session = std::move(session_result.value());
  overlay_state.capture_state.active = false;

  Logger().info("Capture system initialized successfully");
  return {};
}

auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& session = state.overlay->capture_state.session;

  auto start_result = ::Utils::Graphics::Capture::start_capture(session);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected("Failed to start capture");
  }

  state.overlay->capture_state.active = true;
  Logger().debug("Capture started successfully");
  return {};
}

auto stop_capture(Core::State::AppState& state) -> void {
  auto& session = state.overlay->capture_state.session;

  if (state.overlay->capture_state.active) {
    ::Utils::Graphics::Capture::stop_capture(session);
    state.overlay->capture_state.active = false;
    Logger().debug("Capture stopped");
  }
}

auto cleanup_capture(Core::State::AppState& state) -> void {
  auto& session = state.overlay->capture_state.session;

  if (state.overlay->capture_state.active) {
    ::Utils::Graphics::Capture::stop_capture(session);
  }

  ::Utils::Graphics::Capture::cleanup_capture_session(session);
  state.overlay->capture_state.active = false;
  Logger().info("Capture resources cleaned up");
}

}  // namespace Features::Overlay::Capture
