module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <functional>
#include <iostream>

module Features.Overlay.Capture;

import std;
import Core.State;
import Features.Overlay.State;
import Features.Overlay.Rendering;
import Features.Overlay.Utils;
import Features.Overlay.Window;
import Utils.Logger;
import Utils.Graphics.Capture;

namespace Features::Overlay::Capture {

auto initialize_capture(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 检查系统支持
  if (!::Utils::Graphics::Capture::is_capture_supported()) {
    auto error_msg = "Graphics Capture not supported on this system";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 创建WinRT设备
  if (auto result = create_winrt_device(state); !result) {
    Logger().error(std::format("Failed to create WinRT device: {}", result.error()));
    return std::unexpected(result.error());
  }

  Logger().info("Capture system initialized successfully");
  return {};
}

auto start_capture(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!target_window || !IsWindow(target_window)) {
    auto error_msg = "Invalid target window";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 停止现有捕获
  stop_capture(state);

  overlay_state.window.target_window = target_window;

  // 获取窗口尺寸
  auto dimensions_result = Utils::get_window_dimensions(target_window);
  if (!dimensions_result) {
    return std::unexpected(dimensions_result.error());
  }

  auto [width, height] = dimensions_result.value();

  // 更新叠加层窗口尺寸
  if (auto result = Window::update_overlay_window_size(state, width, height); !result) {
    return std::unexpected(result.error());
  }

  // 创建帧回调
  auto frame_callback = [&state](Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) {
    handle_frame_arrived(state, frame_texture);
  };

  // 创建捕获会话
  auto session_result = ::Utils::Graphics::Capture::create_capture_session(
      target_window, overlay_state.capture.session.winrt_device,
      overlay_state.window.cached_game_width, overlay_state.window.cached_game_height,
      frame_callback);

  if (!session_result) {
    return std::unexpected(session_result.error());
  }

  overlay_state.capture.session = std::move(session_result.value());

  // 开始捕获
  auto start_result = ::Utils::Graphics::Capture::start_capture(overlay_state.capture.session);
  if (!start_result) {
    Logger().error(std::format("Failed to start capture: {}", start_result.error()));
    return std::unexpected(start_result.error());
  }

  overlay_state.capture.active = true;
  Logger().info("Capture started successfully");
  return {};
}

auto stop_capture(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  if (overlay_state.capture.active) {
    ::Utils::Graphics::Capture::stop_capture(overlay_state.capture.session);
    overlay_state.capture.active = false;
  }
}

auto cleanup_capture(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  stop_capture(state);
  ::Utils::Graphics::Capture::cleanup_capture_session(overlay_state.capture.session);
  overlay_state.window.target_window = nullptr;
}

auto is_capturing(const Core::State::AppState& state) -> bool {
  return state.overlay->capture.active;
}

auto is_capture_supported() -> bool { return ::Utils::Graphics::Capture::is_capture_supported(); }

auto create_winrt_device(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!overlay_state.rendering.d3d_initialized) {
    return std::unexpected("D3D not initialized");
  }

  auto result = ::Utils::Graphics::Capture::create_winrt_device(
      overlay_state.rendering.d3d_context.device.Get());

  if (!result) {
    return std::unexpected(result.error());
  }

  overlay_state.capture.session.winrt_device = result.value();
  return {};
}

auto handle_frame_arrived(Core::State::AppState& state,
                          Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void {
  auto& overlay_state = *state.overlay;

  // 更新纹理资源
  if (overlay_state.rendering.create_new_srv) {
    if (auto result = Rendering::update_texture_resources(state, frame_texture); result) {
      overlay_state.rendering.create_new_srv = false;
    }
  }

  // 通知渲染系统有新帧
  Rendering::on_frame_arrived(state, frame_texture);
}

}  // namespace Features::Overlay::Capture
