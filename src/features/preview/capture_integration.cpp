module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <functional>
#include <iostream>

module Features.Preview.CaptureIntegration;

import std;
import Features.Preview.State;
import Core.State;
import Utils.Graphics.Capture;
import Utils.Logger;
import Features.Preview.Rendering;

// 简化命名空间内容，使用统一的类型定义

namespace Features::Preview::CaptureIntegration {

auto initialize_capture(Core::State::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查是否支持捕获
  if (!Utils::Graphics::Capture::is_capture_supported()) {
    return std::unexpected("Capture not supported on this system");
  }

  // 确保渲染系统已初始化
  auto* rendering_resources = Features::Preview::Rendering::get_rendering_resources(state);
  if (!rendering_resources->initialized) {
    return std::unexpected("D3D not initialized");
  }

  // 创建WinRT设备
  auto winrt_device_result =
      Utils::Graphics::Capture::create_winrt_device(rendering_resources->d3d_context.device.Get());
  if (!winrt_device_result) {
    Logger().error("Failed to create WinRT device for capture");
    return std::unexpected("Failed to create WinRT device");
  }

  // 创建帧回调
  auto frame_callback = [&state](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
    on_frame_arrived(state, texture);
  };

  // 创建捕获会话
  auto session_result = Utils::Graphics::Capture::create_capture_session(
      target_window, winrt_device_result.value(), width, height, frame_callback);

  if (!session_result) {
    Logger().error("Failed to create capture session");
    return std::unexpected("Failed to create capture session");
  }

  state.preview->capture_session.session = std::move(session_result.value());
  state.preview->capture_session.active = false;

  Logger().info("Capture system initialized successfully");
  return {};
}

auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& capture = state.preview->capture_session.session;

  auto start_result = Utils::Graphics::Capture::start_capture(capture);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected("Failed to start capture");
  }

  state.preview->capture_session.active = true;
  Logger().info("Capture started successfully");
  return {};
}

auto stop_capture(Core::State::AppState& state) -> void {
  auto& capture = state.preview->capture_session.session;

  if (state.preview->capture_session.active) {
    Utils::Graphics::Capture::stop_capture(capture);
    state.preview->capture_session.active = false;
    Logger().info("Capture stopped");
  }
}

auto cleanup_capture(Core::State::AppState& state) -> void {
  auto& capture = state.preview->capture_session.session;

  if (state.preview->capture_session.active) {
    Utils::Graphics::Capture::stop_capture(capture);
  }

  Utils::Graphics::Capture::cleanup_capture_session(capture);
  state.preview->capture_session.active = false;

  Logger().info("Capture resources cleaned up");
}

auto on_frame_arrived(Core::State::AppState& state, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> void {
  if (!state.preview->running || !texture) {
    return;
  }

  // 触发渲染
  Features::Preview::Rendering::render_frame(state, texture);
}

auto is_capture_active(const Core::State::AppState& state) -> bool {
  return state.preview->capture_session.active;
}

auto get_capture_session(Core::State::AppState& state) -> CaptureSession* {
  return &state.preview->capture_session;
}

}  // namespace Features::Preview::CaptureIntegration