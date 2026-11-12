#include "features/preview/preview.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"
#include "vendor/windows/windowsx.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "features/preview/capture.hpp"
#include "features/preview/interaction.hpp"
#include "features/preview/rendering.hpp"
#include "features/preview/state.hpp"
#include "features/preview/types.hpp"
#include "features/preview/viewport.hpp"
#include "features/preview/window.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "utils/display/display.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/graphics/d3d.hpp"
#include "utils/graphics/hdr.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::preview {

auto set_working_monitor(core::AppState& state, const utils::display::MonitorInfo& monitor)
    -> void {
  state.preview->screen_rect = monitor.monitor_rect;
  state.preview->has_screen_rect = true;
  state.preview->interaction.last_game_window_pos.reset();
  viewport::update_viewport_rect(state);
}

auto start_preview(core::AppState& state, HWND target_window,
                   const utils::display::MonitorInfo& monitor_info)
    -> std::expected<void, std::string> {
  auto& preview_state = *state.preview;

  // 检查是否支持捕获
  if (!state.runtime_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
  }

  // 检查预览窗口是否存在，如果不存在则初始化
  if (!preview_state.hwnd) {
    HINSTANCE instance = GetModuleHandle(nullptr);

    if (auto result = window::initialize_preview_window(state, instance); !result) {
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

  auto hdr_info = utils::graphics::hdr::query_monitor_hdr_info(target_window);
  if (!hdr_info) {
    return std::unexpected("Failed to query HDR monitor info: " + hdr_info.error());
  }
  const bool enable_hdr = hdr_info->hdr_active;

  KillTimer(preview_state.hwnd, TIMER_ID_PREVIEW_CLEANUP);

  // 启动失败也统一释放部分初始化的资源，避免留下可见但没有捕获会话的窗口。
  auto rollback = wil::scope_exit([&] {
    preview_state.running.store(false, std::memory_order_release);
    capture::cleanup_capture(state);
    window::hide_preview_window(state);
    rendering::cleanup_rendering(state);
  });

  // 保存目标窗口
  preview_state.target_window = target_window;
  set_working_monitor(state, monitor_info);

  if (preview_state.rendering_resources.initialized.load(std::memory_order_acquire) &&
      preview_state.rendering_resources.d3d_context.enable_hdr != enable_hdr) {
    rendering::cleanup_rendering(state);
  }
  preview_state.enable_hdr = enable_hdr;

  // 使用 WGC 的实际捕获尺寸，避免客户区与捕获范围不一致导致首次就重建帧池。
  auto capture_size = utils::graphics::capture::get_capture_item_size(target_window);
  if (!capture_size) {
    return std::unexpected(capture_size.error());
  }
  const auto [width, height] = *capture_size;

  // 初始化渲染系统（如果需要）
  if (!preview_state.rendering_resources.initialized.load(std::memory_order_acquire)) {
    auto rendering_result =
        rendering::initialize_rendering(state, preview_state.hwnd, preview_state.size.window_width,
                                        preview_state.size.window_height);

    if (!rendering_result) {
      Logger().error("Failed to initialize rendering system");
      return std::unexpected(rendering_result.error());
    }
  }

  // 初始化捕获系统
  auto capture_result = capture::initialize_capture(state, target_window, width, height);

  if (!capture_result) {
    Logger().error("Failed to initialize capture system");
    return std::unexpected(capture_result.error());
  }
  // 计算窗口尺寸和宽高比
  window::set_preview_window_size(state, width, height);
  if (preview_state.rendering_resources.resize_error) {
    return std::unexpected(*preview_state.rendering_resources.resize_error);
  }

  window::show_preview_window(state);

  // 启动捕获
  preview_state.running.store(true, std::memory_order_release);
  auto start_result = capture::start_capture(state);
  if (!start_result) {
    Logger().error("Failed to start capture");
    return std::unexpected(start_result.error());
  }

  rollback.release();

  Logger().info("Preview capture started successfully");
  return {};
}

auto stop_preview(core::AppState& state) -> void {
  auto& preview_state = *state.preview;

  if (!preview_state.running.load(std::memory_order_acquire)) {
    return;
  }

  preview_state.running.store(false, std::memory_order_release);

  interaction::cancel_interaction(state);

  // 停止捕获
  capture::stop_capture(state);

  // 隐藏窗口
  window::hide_preview_window(state);

  if (SetTimer(preview_state.hwnd, TIMER_ID_PREVIEW_CLEANUP, 3000, nullptr) == 0) {
    Logger().error("Failed to schedule delayed preview cleanup");
    cleanup_preview(state);
  }

  Logger().info("Preview capture stopped");
}

auto fail_preview(core::AppState& state, const std::string& error) -> void {
  Logger().error("Preview session {} failed: {}",
                 state.preview->capture_state.generation.load(std::memory_order_acquire), error);
  stop_preview(state);

  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = utils::string::FromUtf8(state.i18n->texts["message.preview_failed"] + error);
  core::notifications::post_notification_request(state, std::move(options));
  ui::floating_window::request_repaint(state);
}

auto update_preview_dpi(core::AppState& state, UINT new_dpi) -> void {
  state.preview->dpi_sizes.update_dpi_scaling(new_dpi);
  window::update_preview_window_dpi(state, new_dpi);
}

auto cleanup_preview(core::AppState& state) -> void {
  if (state.preview->hwnd) {
    KillTimer(state.preview->hwnd, TIMER_ID_PREVIEW_CLEANUP);
  }
  capture::cleanup_capture(state);
  rendering::cleanup_rendering(state);

  Logger().info("Preview resources cleaned up");
}

}  // namespace features::preview
