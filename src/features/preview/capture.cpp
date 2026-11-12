#include "features/preview/capture.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/winrt/windows_foundation.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "features/preview/rendering.hpp"
#include "features/preview/state.hpp"
#include "features/preview/types.hpp"
#include "features/preview/window.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::preview::capture {

auto pack_extent(int width, int height) -> std::uint64_t {
  // 将尺寸编码进窗口消息，避免为一次尺寸通知分配额外对象。
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(width)) << 32) |
         static_cast<std::uint32_t>(height);
}

auto report_capture_failure(core::AppState& state,
                            const std::shared_ptr<CaptureNotification>& notification, HWND hwnd,
                            WPARAM generation, const std::string& error) -> void {
  if (!notification->active.exchange(false, std::memory_order_acq_rel)) {
    return;
  }

  // 失败提示可跨线程投递；会话停止和窗口刷新留给预览窗口线程处理。
  Logger().error("Preview session {} failed: {}", generation, error);
  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = utils::string::FromUtf8(state.i18n->texts["message.preview_failed"] + error);
  core::notifications::post_notification_request(state, std::move(options));
  if (!PostMessageW(hwnd, WM_PREVIEW_CAPTURE_FAILED, generation, 0)) {
    Logger().warn("Failed to post preview failure message for session {}: {}", generation,
                  GetLastError());
  }
}

auto on_frame_arrived(core::AppState& state,
                      const std::shared_ptr<CaptureNotification>& notification, HWND hwnd,
                      WPARAM generation, utils::graphics::capture::Direct3D11CaptureFrame frame)
    -> void {
  if (!notification->active.load(std::memory_order_acquire) ||
      !state.preview->running.load(std::memory_order_acquire) ||
      generation != state.preview->capture_state.generation.load(std::memory_order_acquire) ||
      !frame) {
    return;
  }

  try {
    auto close_frame = wil::scope_exit([&] {
      try {
        frame.Close();
      } catch (const winrt::hresult_error& error) {
        Logger().warn("Failed to close preview frame: {}", winrt::to_string(error.message()));
      }
    });
    const auto content = frame.ContentSize();
    if (content.Width <= 0 || content.Height <= 0) {
      return;
    }

    const auto extent = pack_extent(content.Width, content.Height);
    // 同一尺寸只通知一次；尺寸未应用前丢弃帧，避免用旧帧池绘制新尺寸画面。
    if (notification->last_reported_extent.exchange(extent, std::memory_order_acq_rel) != extent) {
      if (!PostMessageW(hwnd, WM_PREVIEW_CAPTURE_SIZE_CHANGED, generation,
                        static_cast<LPARAM>(extent))) {
        notification->last_reported_extent.store(
            state.preview->capture_state.applied_extent.load(std::memory_order_acquire),
            std::memory_order_release);
        report_capture_failure(
            state, notification, hwnd, generation,
            std::format("Failed to post preview size change: {}", GetLastError()));
        return;
      }
    }

    if (extent != state.preview->capture_state.applied_extent.load(std::memory_order_acquire)) {
      return;
    }

    auto surface = frame.Surface();
    auto texture =
        utils::graphics::capture::get_dxgi_interface_from_object<ID3D11Texture2D>(surface);
    if (!texture) {
      report_capture_failure(state, notification, hwnd, generation,
                             "Cannot get preview capture texture");
      return;
    }
    if (auto result = rendering::render_frame(state, texture.get()); !result) {
      report_capture_failure(state, notification, hwnd, generation, result.error());
    }
  } catch (const winrt::hresult_error& error) {
    report_capture_failure(
        state, notification, hwnd, generation,
        std::format("Preview capture failed: {}", winrt::to_string(error.message())));
  } catch (const std::exception& error) {
    report_capture_failure(state, notification, hwnd, generation,
                           std::format("Preview capture failed: {}", error.what()));
  } catch (...) {
    report_capture_failure(state, notification, hwnd, generation,
                           "Preview capture failed with an unknown error");
  }
}

auto cancel_pending_resize(core::AppState& state) -> void {
  auto& preview = *state.preview;
  auto& capture = preview.capture_state;
  if (preview.hwnd) {
    KillTimer(preview.hwnd, TIMER_ID_CAPTURE_RESIZE);
  }
  capture.pending_width = 0;
  capture.pending_height = 0;
  capture.resize_due = {};
}

auto initialize_capture(core::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }
  // 检查系统是否支持 Windows Graphics Capture。
  if (!state.runtime_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 捕获设备复用预览渲染系统的 D3D 设备。
  auto& rendering_resources = state.preview->rendering_resources;
  if (!rendering_resources.initialized.load(std::memory_order_acquire)) {
    return std::unexpected("D3D not initialized");
  }
  // 创建供 WGC 使用的 WinRT D3D 设备。
  auto winrt_device =
      utils::graphics::capture::create_winrt_device(rendering_resources.d3d_context.device.get());
  if (!winrt_device) {
    return std::unexpected(winrt_device.error());
  }

  auto& capture = state.preview->capture_state;
  const auto generation = capture.generation.fetch_add(1, std::memory_order_acq_rel) + 1;
  const auto hwnd = state.preview->hwnd;
  auto notification = std::make_shared<CaptureNotification>();
  notification->last_reported_extent.store(pack_extent(width, height), std::memory_order_release);
  capture.applied_extent.store(pack_extent(width, height), std::memory_order_release);
  // 回调捕获会话编号，旧会话迟到的回调不会操作新会话。
  auto frame_callback = [&state, notification, hwnd, generation](auto&& frame) {
    on_frame_arrived(state, notification, hwnd, generation, std::move(frame));
  };

  utils::graphics::capture::CaptureSessionOptions options;
  if (state.preview->enable_hdr) {
    options.pixel_format = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // 帧池保持单帧以控制显存占用，WGC 后台回调直接消费并渲染帧。
  auto session = utils::graphics::capture::create_capture_session(
      target_window, *winrt_device, width, height, frame_callback, 1, options);
  if (!session) {
    return std::unexpected(session.error());
  }
  capture.session = std::move(*session);
  capture.notification = std::move(notification);
  capture.pending_width = 0;
  capture.pending_height = 0;
  capture.resize_due = {};

  Logger().info("Preview capture session {} initialized at {}x{}", generation, width, height);
  return {};
}

auto start_capture(core::AppState& state) -> std::expected<void, std::string> {
  return utils::graphics::capture::start_capture(state.preview->capture_state.session);
}

auto queue_resize(core::AppState& state, int width, int height)
    -> std::expected<void, std::string> {
  auto& preview = *state.preview;
  if (!preview.running.load(std::memory_order_acquire)) {
    return {};
  }
  if (width <= 0 || height <= 0) {
    return std::unexpected("Invalid preview capture extent");
  }

  auto& capture = preview.capture_state;
  const auto extent = pack_extent(width, height);
  if (extent == capture.applied_extent.load(std::memory_order_acquire)) {
    cancel_pending_resize(state);
    return {};
  }
  if (width == capture.pending_width && height == capture.pending_height) {
    return {};
  }

  capture.pending_width = width;
  capture.pending_height = height;
  // 连续变化时重置等待时间，只为最终稳定尺寸重建帧池。
  capture.resize_due =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(CAPTURE_RESIZE_DELAY_MS);
  KillTimer(preview.hwnd, TIMER_ID_CAPTURE_RESIZE);
  if (SetTimer(preview.hwnd, TIMER_ID_CAPTURE_RESIZE, CAPTURE_RESIZE_DELAY_MS, nullptr) == 0) {
    return std::unexpected("Failed to schedule preview frame pool resize");
  }
  return {};
}

auto apply_pending_resize(core::AppState& state) -> std::expected<void, std::string> {
  auto& preview = *state.preview;
  if (!preview.running.load(std::memory_order_acquire)) {
    return {};
  }

  auto& capture = preview.capture_state;
  const int width = capture.pending_width;
  const int height = capture.pending_height;
  if (width <= 0 || height <= 0 ||
      pack_extent(width, height) == capture.applied_extent.load(std::memory_order_acquire)) {
    return {};
  }

  // 忽略 KillTimer 后仍可能派发的旧消息，保证尺寸稳定等待期完整。
  const auto remaining = capture.resize_due - std::chrono::steady_clock::now();
  if (remaining > std::chrono::steady_clock::duration::zero()) {
    const auto remaining_ms = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
    if (SetTimer(preview.hwnd, TIMER_ID_CAPTURE_RESIZE,
                 static_cast<UINT>(std::max<std::chrono::milliseconds::rep>(remaining_ms, 1)),
                 nullptr) == 0) {
      return std::unexpected("Failed to reschedule preview frame pool resize");
    }
    return {};
  }

  // 当前帧已归还；重建前释放 SRV，避免旧纹理引用留在帧池之外。
  rendering::release_capture_surface(state);
  if (auto result = utils::graphics::capture::recreate_frame_pool(capture.session, width, height);
      !result) {
    return result;
  }

  capture.pending_width = 0;
  capture.pending_height = 0;
  capture.resize_due = {};
  window::set_preview_window_size(state, width, height);
  if (!preview.running.load(std::memory_order_acquire)) {
    return {};
  }
  if (preview.rendering_resources.resize_error) {
    return std::unexpected(*preview.rendering_resources.resize_error);
  }

  capture.applied_extent.store(pack_extent(width, height), std::memory_order_release);
  Logger().debug("Preview session {} capture resized to {}x{}",
                 capture.generation.load(std::memory_order_acquire), width, height);
  return {};
}

auto stop_capture(core::AppState& state) -> void {
  auto& capture = state.preview->capture_state;
  cancel_pending_resize(state);
  if (capture.notification) {
    capture.notification->active.store(false, std::memory_order_release);
    capture.notification.reset();
  }
  utils::graphics::capture::stop_capture(capture.session);
  Logger().debug("Preview capture session {} stopped",
                 capture.generation.load(std::memory_order_acquire));
}

auto cleanup_capture(core::AppState& state) -> void {
  auto& capture = state.preview->capture_state;
  cancel_pending_resize(state);
  if (capture.notification) {
    capture.notification->active.store(false, std::memory_order_release);
    capture.notification.reset();
  }
  rendering::release_capture_surface(state);
  utils::graphics::capture::cleanup_capture_session(capture.session);
  Logger().info("Preview capture resources cleaned up");
}

}  // namespace features::preview::capture
