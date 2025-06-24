module;

module Handlers.Window;

import std;
import Core.Actions;
import Core.Config.Io;
import Core.Events;
import Core.State;
import Features.WindowControl;
import Features.Notifications;
import UI.AppWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Handlers {

auto handle_ratio_changed(Core::State::AppState& state, const Core::Events::Event& event) -> void;
auto handle_resolution_changed(Core::State::AppState& state, const Core::Events::Event& event)
    -> void;
auto handle_window_action(Core::State::AppState& state, const Core::Events::Event& event) -> void;
auto get_current_ratio(const Core::State::AppState& state) -> double;
auto get_current_total_pixels(const Core::State::AppState& state) -> std::uint64_t;
auto post_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window,
                            const Features::WindowControl::Resolution& resolution) -> void;

// 注册窗口处理器
auto register_window_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 处理比例改变
  subscribe(app_state.event_bus, EventType::RatioChanged,
            [&app_state](const Event& event) { handle_ratio_changed(app_state, event); });

  // 处理分辨率改变
  subscribe(app_state.event_bus, EventType::ResolutionChanged,
            [&app_state](const Event& event) { handle_resolution_changed(app_state, event); });

  // 处理窗口动作
  subscribe(app_state.event_bus, EventType::WindowAction,
            [&app_state](const Event& event) { handle_window_action(app_state, event); });
}

// 处理比例改变事件
auto handle_ratio_changed(Core::State::AppState& state, const Core::Events::Event& event) -> void {
  auto data = std::any_cast<Core::Events::RatioChangeData>(event.data);
  Logger().debug("Handling ratio change to index {}, ratio: {}", data.index, data.ratio_value);

  // 查找目标窗口
  auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
  if (!target_window) {
    Features::Notifications::show_notification(state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    return;
  }

  // 计算新分辨率
  auto total_pixels = get_current_total_pixels(state);
  Features::WindowControl::Resolution new_resolution;

  if (total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(data.ratio_value);
  } else {
    new_resolution = Features::WindowControl::calculate_resolution(data.ratio_value, total_pixels);
  }

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{.taskbar_lower = state.config.taskbar.lower,
                                                    .activate_window = !state.ui.overlay_enabled};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(state, "SpinningMomo", "Failed to apply window transform: " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, *target_window, new_resolution);

  Core::Actions::dispatch_action(
      state, Core::Actions::Action{Core::Actions::Payloads::SetCurrentRatio{.index = data.index}});

  Core::Actions::trigger_ui_update(state);
}

// 处理分辨率改变事件
auto handle_resolution_changed(Core::State::AppState& state, const Core::Events::Event& event)
    -> void {
  auto data = std::any_cast<Core::Events::ResolutionChangeData>(event.data);
  Logger().debug("Handling resolution change to index {}, pixels: {}", data.index,
                 data.total_pixels);

  // 查找目标窗口
  auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
  if (!target_window) {
    Features::Notifications::show_notification(state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    return;
  }

  // 获取当前比例
  double current_ratio = get_current_ratio(state);

  // 计算新分辨率
  Features::WindowControl::Resolution new_resolution;
  if (data.total_pixels == 0) {
    // 默认选项，使用屏幕尺寸
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(current_ratio);
  } else {
    new_resolution =
        Features::WindowControl::calculate_resolution(current_ratio, data.total_pixels);
  }

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{.taskbar_lower = state.config.taskbar.lower,
                                                    .activate_window = !state.ui.overlay_enabled};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(state, "SpinningMomo", "Failed to apply window transform: " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, *target_window, new_resolution);

  Core::Actions::dispatch_action(
      state,
      Core::Actions::Action{Core::Actions::Payloads::SetCurrentResolution{.index = data.index}});

  Core::Actions::trigger_ui_update(state);
}

// 处理窗口动作事件
auto handle_window_action(Core::State::AppState& state, const Core::Events::Event& event) -> void {
  auto action = std::any_cast<Core::Events::WindowAction>(event.data);

  switch (action) {
    case Core::Events::WindowAction::Reset: {
      auto target_window = Features::WindowControl::find_target_window(state.config.window.title);
      if (!target_window) {
        Features::Notifications::show_notification(state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
        return;
      }

      Features::WindowControl::TransformOptions options{.taskbar_lower = state.config.taskbar.lower,
                                                        .activate_window = true};

      auto result = Features::WindowControl::reset_window_to_screen(*target_window, options);
      if (!result) {
        Features::Notifications::show_notification(state, "SpinningMomo", "Failed to reset window: " + result.error());
        return;
      }

      Core::Actions::dispatch_action(
          state, Core::Actions::Action{Core::Actions::Payloads::ResetWindowState{}});

      Core::Actions::trigger_ui_update(state);

      Logger().debug("Window reset to screen size successfully");
      break;
    }
    default:
      // 其他动作暂不处理，保留给其他处理器
      break;
  }
}

// 获取当前比例
auto get_current_ratio(const Core::State::AppState& state) -> double {
  if (state.ui.current_ratio_index < state.data.ratios.size()) {
    return state.data.ratios[state.ui.current_ratio_index].ratio;
  }

  // 默认使用屏幕比例
  int screen_width = Vendor::Windows::GetScreenWidth();
  int screen_height = Vendor::Windows::GetScreenHeight();
  return static_cast<double>(screen_width) / screen_height;
}

// 获取当前总像素数
auto get_current_total_pixels(const Core::State::AppState& state) -> std::uint64_t {
  if (state.ui.current_resolution_index < state.data.resolutions.size()) {
    return state.data.resolutions[state.ui.current_resolution_index].totalPixels;
  }
  return 0;  // 表示使用屏幕尺寸
}

// 变换后的后续处理
auto post_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window,
                            const Features::WindowControl::Resolution& resolution) -> void {
  // TODO: 添加后续处理逻辑
  // 1. 更新黑边状态
  // 2. 重启窗口捕获
  // 3. 其他UI更新

  Logger().debug("Post-transform actions completed for window {}x{}", resolution.width,
                 resolution.height);
}

}  // namespace Handlers