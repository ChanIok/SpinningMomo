#include "features/overlay/overlay.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"

#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "features/overlay/capture.hpp"
#include "features/overlay/geometry.hpp"
#include "features/overlay/interaction.hpp"
#include "features/overlay/rendering.hpp"
#include "features/overlay/state.hpp"
#include "features/overlay/threads.hpp"
#include "features/overlay/types.hpp"
#include "features/overlay/window.hpp"
#include "ui/floating_window/state.hpp"
#include "utils/display/display.hpp"
#include "utils/graphics/hdr.hpp"
#include "utils/logger/logger.hpp"

namespace features::overlay {
auto send_overlay_control_message(HWND overlay_hwnd, UINT message) -> bool {
  if (!overlay_hwnd || !IsWindow(overlay_hwnd)) {
    return false;
  }

  return SendMessageW(overlay_hwnd, message, 0, 0) != 0;
}

auto cleanup_overlay(core::AppState& state) -> void {
  if (send_overlay_control_message(state.overlay->window.overlay_hwnd,
                                   WM_IMMEDIATE_OVERLAY_CLEANUP)) {
    return;
  }

  capture::cleanup_capture(state);
  rendering::cleanup_rendering(state);

  Logger().info("Overlay cleaned up");
}

auto schedule_overlay_cleanup(core::AppState& state) -> void {
  if (!send_overlay_control_message(state.overlay->window.overlay_hwnd,
                                    WM_SCHEDULE_OVERLAY_CLEANUP)) {
    cleanup_overlay(state);
  }
}

auto stop_overlay_runtime(core::AppState& state, bool restore_target_window) -> void {
  auto& overlay_state = *state.overlay;

  overlay_state.running.store(false, std::memory_order_release);
  overlay_state.freeze_rendering.store(false, std::memory_order_release);
  overlay_state.freeze_after_first_frame.store(false, std::memory_order_release);
  overlay_state.rendering.create_new_srv = true;

  threads::stop_threads(state);
  capture::stop_capture(state);

  if (restore_target_window) {
    window::restore_game_window(state);
  }

  window::hide_overlay_window(state);
  overlay_state.window.target_window = nullptr;

  threads::wait_for_threads(state);
  interaction::cleanup_interaction(state);
}

auto start_overlay(core::AppState& state, HWND target_window, bool freeze_after_first_frame)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (state.overlay->running.load(std::memory_order_acquire)) {
    Logger().debug("Overlay already running, skipping");
    return {};
  }

  // 检查是否支持捕捉
  if (!state.runtime_info->is_capture_supported) {
    return std::unexpected("Capture not supported on this system");
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

  const auto& fw = *state.floating_window;
  auto monitor_info = utils::display::get_working_monitor(fw.window.hwnd, fw.window.is_visible);
  if (!monitor_info) {
    return std::unexpected("Failed to resolve working monitor: " + monitor_info.error());
  }

  overlay_state.window.target_window = target_window;
  if (overlay_state.rendering.d3d_initialized &&
      overlay_state.rendering.d3d_context.enable_hdr != enable_hdr) {
    rendering::cleanup_rendering(state);
  }
  overlay_state.enable_hdr = enable_hdr;

  const auto& monitor_rect = monitor_info->monitor_rect;
  overlay_state.window.screen_left = monitor_rect.left;
  overlay_state.window.screen_top = monitor_rect.top;
  overlay_state.window.screen_width = utils::display::rect_width(monitor_rect);
  overlay_state.window.screen_height = utils::display::rect_height(monitor_rect);

  // 检查窗口是否已初始化，如果未初始化则进行初始化
  if (!overlay_state.window.overlay_hwnd) {
    HINSTANCE instance = GetModuleHandle(nullptr);

    if (auto result = window::initialize_overlay_window(state, instance); !result) {
      overlay_state.window.target_window = nullptr;
      return std::unexpected(result.error());
    }
  }

  // 获取窗口尺寸
  auto dimensions_result = geometry::get_window_dimensions(target_window);
  if (!dimensions_result) {
    return std::unexpected(dimensions_result.error());
  }

  auto [width, height] = dimensions_result.value();
  auto screen_width = utils::display::rect_width(monitor_info->monitor_rect);
  auto screen_height = utils::display::rect_height(monitor_info->monitor_rect);

  // 在非变换场景下，检查是否需要 overlay
  if (!freeze_after_first_frame &&
      !geometry::should_use_overlay(width, height, screen_width, screen_height)) {
    overlay_state.window.target_window = nullptr;
    // 不返回错误，因为游戏窗口在屏幕内，不需要叠加层
    return {};
  }

  // 设置首帧后自动冻结标志
  overlay_state.freeze_after_first_frame.store(freeze_after_first_frame, std::memory_order_release);

  if (!send_overlay_control_message(overlay_state.window.overlay_hwnd, WM_CANCEL_OVERLAY_CLEANUP)) {
    Logger().warn("Failed to cancel pending overlay cleanup");
  }

  overlay_state.interaction.last_game_window_pos.reset();

  // 更新窗口尺寸
  window::set_overlay_window_size(state, width, height);

  // 初始化渲染系统（仅在未初始化时）
  if (!overlay_state.rendering.d3d_initialized) {
    if (auto result = rendering::initialize_rendering(state); !result) {
      return std::unexpected(result.error());
    }
  }

  overlay_state.running.store(true, std::memory_order_release);  // 设置运行状态为true

  // 初始化捕获
  if (auto result = capture::initialize_capture(state, target_window, width, height); !result) {
    overlay_state.running.store(false, std::memory_order_release);
    return std::unexpected(result.error());
  }

  // 启动捕获
  if (auto result = capture::start_capture(state); !result) {
    overlay_state.running.store(false, std::memory_order_release);
    return std::unexpected(result.error());
  }

  // 启动线程（只启动钩子和窗口管理线程）
  if (auto result = threads::start_threads(state); !result) {
    overlay_state.running.store(
        false, std::memory_order_release);  // 如果线程启动失败，设置运行状态为false
    return std::unexpected(result.error());
  }

  return {};
}

auto stop_overlay(core::AppState& state, bool restore_target_window) -> void {
  Logger().debug("Stopping overlay");

  stop_overlay_runtime(state, restore_target_window);
  schedule_overlay_cleanup(state);

  Logger().debug("Overlay stopped");
}

auto freeze_overlay(core::AppState& state) -> void {
  state.overlay->freeze_rendering.store(true, std::memory_order_release);
  Logger().debug("Overlay frozen");
}

auto unfreeze_overlay(core::AppState& state) -> void {
  state.overlay->freeze_rendering.store(false, std::memory_order_release);
  state.overlay->freeze_after_first_frame.store(false, std::memory_order_release);
  Logger().debug("Overlay unfrozen");
}

auto set_letterbox_mode(core::AppState& state, bool enabled) -> void {
  state.overlay->window.use_letterbox_mode = enabled;
}

}  // namespace features::overlay
