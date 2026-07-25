#include "features/preview/capture.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"

#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "features/preview/rendering.hpp"
#include "features/preview/state.hpp"
#include "features/preview/types.hpp"
#include "features/preview/window.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/logger/logger.hpp"

namespace features::preview::capture {

auto on_frame_arrived(core::AppState& state, utils::graphics::capture::Direct3D11CaptureFrame frame)
    -> void {
  if (!state.preview->running.load(std::memory_order_acquire) || !frame) {
    return;
  }

  // 检查帧大小是否发生变化
  auto content_size = frame.ContentSize();
  const auto last_width =
      state.preview->capture_state.last_frame_width.load(std::memory_order_acquire);
  const auto last_height =
      state.preview->capture_state.last_frame_height.load(std::memory_order_acquire);

  bool size_changed = (content_size.Width != last_width) || (content_size.Height != last_height);

  if (size_changed) {
    // 捕获回调线程只负责发现尺寸变化；帧池重建与窗口尺寸应用统一收口到窗口线程。
    if (!PostMessageW(state.preview->hwnd, features::preview::WM_APPLY_CAPTURE_SIZE,
                      static_cast<WPARAM>(content_size.Width),
                      static_cast<LPARAM>(content_size.Height))) {
      Logger().warn("Failed to post preview capture size update message");
    }

    return;
  }

  auto surface = frame.Surface();
  if (surface) {
    auto texture =
        utils::graphics::capture::get_dxgi_interface_from_object<ID3D11Texture2D>(surface);
    if (texture) {
      // 触发渲染
      features::preview::rendering::render_frame(state, texture);
    }
  }
}

auto initialize_capture(core::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 检查是否支持捕获
  if (!state.runtime_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 确保渲染系统已初始化
  auto& rendering_resources = state.preview->rendering_resources;
  if (!rendering_resources.initialized.load(std::memory_order_acquire)) {
    return std::unexpected("D3D not initialized");
  }

  // 创建WinRT设备
  auto winrt_device_result =
      utils::graphics::capture::create_winrt_device(rendering_resources.d3d_context.device.get());
  if (!winrt_device_result) {
    Logger().error("Failed to create WinRT device for capture");
    return std::unexpected("Failed to create WinRT device");
  }

  // 创建帧回调
  auto frame_callback = [&state](winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame) {
    on_frame_arrived(state, frame);
  };

  utils::graphics::capture::CaptureSessionOptions capture_options;
  if (state.preview->enable_hdr) {
    capture_options.pixel_format =
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // 创建捕获会话
  auto session_result = utils::graphics::capture::create_capture_session(
      target_window, winrt_device_result.value(), width, height, frame_callback, 1,
      capture_options);

  if (!session_result) {
    Logger().error("Failed to create capture session");
    return std::unexpected("Failed to create capture session");
  }

  state.preview->capture_state.session = std::move(session_result.value());
  state.preview->capture_state.last_frame_width.store(width, std::memory_order_release);
  state.preview->capture_state.last_frame_height.store(height, std::memory_order_release);

  Logger().info("Capture system initialized successfully");
  return {};
}

auto start_capture(core::AppState& state) -> std::expected<void, std::string> {
  auto& session = state.preview->capture_state.session;

  auto start_result = utils::graphics::capture::start_capture(session);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected("Failed to start capture");
  }

  Logger().debug("Capture started successfully");
  return {};
}

auto stop_capture(core::AppState& state) -> void {
  auto& session = state.preview->capture_state.session;

  utils::graphics::capture::stop_capture(session);
  Logger().debug("Capture stopped");
}

auto cleanup_capture(core::AppState& state) -> void {
  auto& session = state.preview->capture_state.session;

  utils::graphics::capture::cleanup_capture_session(session);

  Logger().info("Capture resources cleaned up");
}

}  // namespace features::preview::capture
