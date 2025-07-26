module;

#include <windows.h>

#include <iostream>

module Features.Overlay;

import std;
import Core.State;
import Features.Overlay.State;
import Features.Overlay.Window;
import Features.Overlay.Rendering;
import Features.Overlay.Capture;
import Features.Overlay.Interaction;
import Features.Overlay.Threads;
import Features.Overlay.Utils;
import Utils.Logger;

namespace Features::Overlay {

auto initialize_overlay(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string> {
  // 设置全局状态指针
  Utils::set_global_app_state(&state);

  // 初始化窗口系统
  if (auto result = Window::initialize_overlay_window(state, instance, parent); !result) {
    return std::unexpected(result.error());
  }

  return {};
}

auto start_overlay(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window");
  }

  // 停止现有的叠加层
  stop_overlay(state);

  // 获取窗口尺寸并检查是否需要叠加层
  auto dimensions_result = Utils::get_window_dimensions(target_window);
  if (!dimensions_result) {
    return std::unexpected(dimensions_result.error());
  }

  auto [width, height] = dimensions_result.value();
  auto [screen_width, screen_height] = Utils::get_screen_dimensions();

  if (!Utils::should_use_overlay(width, height, screen_width, screen_height)) {
    Window::restore_game_window(state, true);
    return std::unexpected("Game window fits within screen, overlay not needed");
  }

  // 更新窗口尺寸
  if (auto result = Window::update_overlay_window_size(state, width, height); !result) {
    return std::unexpected(result.error());
  }

  // 验证overlay窗口已创建且有效
  if (!overlay_state.window.overlay_hwnd || !IsWindow(overlay_state.window.overlay_hwnd)) {
    return std::unexpected("Overlay window is not valid. Please restart the application.");
  }

  // 初始化D3D渲染
  if (auto result = Rendering::initialize_d3d_rendering(state); !result) {
    return std::unexpected(result.error());
  }

  // 设置目标窗口
  overlay_state.window.target_window = target_window;
  overlay_state.running = true;  // 设置运行状态为true

  // 启动线程
  if (auto result = Threads::start_threads(state); !result) {
    overlay_state.running = false;  // 如果线程启动失败，设置运行状态为false
    return std::unexpected(result.error());
  }

  return {};
}

auto stop_overlay(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  Logger().debug("Stopping overlay");

  // 设置运行状态为false
  overlay_state.running = false;

  // 停止线程
  Threads::stop_threads(state);

  // 停止捕获
  Capture::stop_capture(state);

  // 隐藏窗口
  Window::hide_overlay_window(state);

  // 恢复游戏窗口
  Window::restore_game_window(state);

  // 等待线程结束
  Threads::wait_for_threads(state);

  // 清理交互资源
  Interaction::cleanup_interaction(state);

  Logger().debug("Overlay stopped");
}

auto set_letterbox_mode(Core::State::AppState& state, bool enabled) -> void {
  state.overlay->window.use_letterbox_mode = enabled;
}

auto cleanup_overlay(Core::State::AppState& state) -> void {
  // 停止所有活动
  stop_overlay(state);

  // 清理渲染资源
  Rendering::cleanup_rendering_resources(state);

  // 清理捕获资源
  Capture::cleanup_capture(state);

  // 销毁窗口
  Window::destroy_overlay_window(state);

  // 注销窗口类
  Utils::unregister_overlay_window_class(GetModuleHandle(nullptr));
}

}  // namespace Features::Overlay
