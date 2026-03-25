module;

module Features.Overlay;

import std;
import Core.State;
import Core.State.RuntimeInfo;
import Features.Overlay.State;
import Features.Overlay.Types;
import Features.Overlay.Window;
import Features.Overlay.Rendering;
import Features.Overlay.Capture;
import Features.Overlay.Interaction;
import Features.Overlay.Threads;
import Features.Overlay.Geometry;
import Utils.Logger;
import <dwmapi.h>;
import <windows.h>;

namespace Features::Overlay {

auto send_overlay_control_message(HWND overlay_hwnd, UINT message) -> bool {
  if (!overlay_hwnd || !IsWindow(overlay_hwnd)) {
    return false;
  }

  return SendMessageW(overlay_hwnd, message, 0, 0) != 0;
}

auto start_overlay(Core::State::AppState& state, HWND target_window, bool freeze_after_first_frame)
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

  // 检查窗口是否已初始化，如果未初始化则进行初始化
  if (!overlay_state.window.overlay_hwnd) {
    HINSTANCE instance = GetModuleHandle(nullptr);

    if (auto result = Window::initialize_overlay_window(state, instance); !result) {
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

  // 获取窗口尺寸
  auto dimensions_result = Geometry::get_window_dimensions(target_window);
  if (!dimensions_result) {
    return std::unexpected(dimensions_result.error());
  }

  auto [width, height] = dimensions_result.value();
  auto [screen_width, screen_height] = Geometry::get_screen_dimensions();

  // 在非变换场景下，检查是否需要 overlay
  if (!freeze_after_first_frame &&
      !Geometry::should_use_overlay(width, height, screen_width, screen_height)) {
    Window::restore_game_window(state);
    // 不返回错误，因为游戏窗口在屏幕内，不需要叠加层
    return {};
  }

  // 设置首帧后自动冻结标志
  overlay_state.freeze_after_first_frame.store(freeze_after_first_frame, std::memory_order_release);

  if (!send_overlay_control_message(overlay_state.window.overlay_hwnd,
                                    Types::WM_CANCEL_OVERLAY_CLEANUP)) {
    Logger().warn("Failed to cancel pending overlay cleanup");
  }

  // 设置目标窗口
  overlay_state.window.target_window = target_window;

  // 更新窗口尺寸
  Window::set_overlay_window_size(state, width, height);

  // 初始化渲染系统（仅在未初始化时）
  if (!overlay_state.rendering.d3d_initialized) {
    if (auto result = Rendering::initialize_rendering(state); !result) {
      return std::unexpected(result.error());
    }
  }

  overlay_state.running.store(true, std::memory_order_release);  // 设置运行状态为true

  // 初始化捕获
  if (auto result = Capture::initialize_capture(state, target_window, width, height); !result) {
    overlay_state.running.store(false, std::memory_order_release);
    return std::unexpected(result.error());
  }

  // 启动捕获
  if (auto result = Capture::start_capture(state); !result) {
    overlay_state.running.store(false, std::memory_order_release);
    return std::unexpected(result.error());
  }

  // 启动线程（只启动钩子和窗口管理线程）
  if (auto result = Threads::start_threads(state); !result) {
    overlay_state.running.store(
        false, std::memory_order_release);  // 如果线程启动失败，设置运行状态为false
    return std::unexpected(result.error());
  }

  return {};
}

auto stop_overlay(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  Logger().debug("Stopping overlay");

  // 设置运行状态为false
  overlay_state.running.store(false, std::memory_order_release);
  overlay_state.freeze_rendering.store(false, std::memory_order_release);
  overlay_state.freeze_after_first_frame.store(false, std::memory_order_release);
  overlay_state.rendering.create_new_srv = true;

  // 停止线程
  Threads::stop_threads(state);

  // 停止捕获
  Capture::stop_capture(state);

  // 恢复游戏窗口
  Window::restore_game_window(state);

  // 隐藏窗口
  Window::hide_overlay_window(state);

  // 等待线程结束
  Threads::wait_for_threads(state);

  // 清理交互资源
  Interaction::cleanup_interaction(state);

  if (!send_overlay_control_message(overlay_state.window.overlay_hwnd,
                                    Types::WM_SCHEDULE_OVERLAY_CLEANUP)) {
    cleanup_overlay(state);
  }

  Logger().debug("Overlay stopped");
}

auto freeze_overlay(Core::State::AppState& state) -> void {
  state.overlay->freeze_rendering.store(true, std::memory_order_release);
  Logger().debug("Overlay frozen");
}

auto unfreeze_overlay(Core::State::AppState& state) -> void {
  state.overlay->freeze_rendering.store(false, std::memory_order_release);
  state.overlay->freeze_after_first_frame.store(false, std::memory_order_release);
  Logger().debug("Overlay unfrozen");
}

auto set_letterbox_mode(Core::State::AppState& state, bool enabled) -> void {
  state.overlay->window.use_letterbox_mode = enabled;
}

auto cleanup_overlay(Core::State::AppState& state) -> void {
  if (send_overlay_control_message(state.overlay->window.overlay_hwnd,
                                   Types::WM_IMMEDIATE_OVERLAY_CLEANUP)) {
    return;
  }

  Capture::cleanup_capture(state);
  Rendering::cleanup_rendering(state);

  Logger().info("Overlay cleaned up");
}

}  // namespace Features::Overlay
