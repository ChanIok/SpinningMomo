#include "features/window_control/usecase.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/async/ui_awaitable.hpp"
#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
#include "features/letterbox/letterbox.hpp"
#include "features/letterbox/state.hpp"
#include "features/overlay/geometry.hpp"
#include "features/overlay/interaction.hpp"
#include "features/overlay/overlay.hpp"
#include "features/overlay/state.hpp"
#include "features/preview/preview.hpp"
#include "features/preview/state.hpp"
#include "features/settings/menu.hpp"
#include "features/settings/settings.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/types.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/floating_window/events.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/floating_window/state.hpp"
#include "utils/display/display.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::window_control {

// 获取当前比例
auto get_current_ratio(const core::AppState& state, const utils::display::MonitorInfo& monitor_info)
    -> double {
  const auto& ratios = features::settings::menu::get_ratios(state);
  if (state.floating_window->ui.current_ratio_index < ratios.size()) {
    return ratios[state.floating_window->ui.current_ratio_index].ratio;
  }

  // 默认使用工作显示器比例。
  int screen_width = utils::display::rect_width(monitor_info.monitor_rect);
  int screen_height = utils::display::rect_height(monitor_info.monitor_rect);
  return static_cast<double>(screen_width) / screen_height;
}

// 将 settings/menu 的预设结构转成窗口控制模块的纯几何输入。
auto to_resolution_preset_input(const features::settings::menu::ResolutionPreset* resolution_preset)
    -> features::window_control::ResolutionPresetInput {
  if (!resolution_preset) {
    return {};
  }

  return features::window_control::ResolutionPresetInput{
      .base_width = resolution_preset->base_width,
      .base_height = resolution_preset->base_height,
  };
}

// 比例切换时沿用当前选中的分辨率预设。
auto get_current_resolution_preset(const core::AppState& state)
    -> features::window_control::ResolutionPresetInput {
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  if (state.floating_window->ui.current_resolution_index < resolutions.size()) {
    return to_resolution_preset_input(
        &resolutions[state.floating_window->ui.current_resolution_index]);
  }

  return {};
}

// 分辨率切换时使用事件指定的预设；越界时回退为 Default。
auto get_resolution_preset_by_index(const core::AppState& state, size_t resolution_index)
    -> features::window_control::ResolutionPresetInput {
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  if (resolution_index < resolutions.size()) {
    return to_resolution_preset_input(&resolutions[resolution_index]);
  }

  return {};
}

// 只提取窗口尺寸计算需要的设置，避免 usecase 参与具体算法。
auto get_resolution_calculation_options(const core::AppState& state,
                                        const utils::display::MonitorInfo& monitor_info)
    -> features::window_control::ResolutionCalculationOptions {
  const auto& window_settings = state.settings->raw.window;
  return features::window_control::ResolutionCalculationOptions{
      .align_to_8 = window_settings.align_window_size_to_8,
      .use_short_edge = window_settings.use_resolution_short_edge,
      .screen_width = utils::display::rect_width(monitor_info.monitor_rect),
      .screen_height = utils::display::rect_height(monitor_info.monitor_rect),
  };
}

// 悬浮窗/托盘菜单产生的尺寸统一从这里进入 WindowControl。
auto calculate_menu_resolution(const core::AppState& state, double ratio,
                               const features::window_control::ResolutionPresetInput& preset,
                               const utils::display::MonitorInfo& monitor_info)
    -> features::window_control::Resolution {
  // usecase 只负责把运行时设置和菜单预设翻译成窗口控制模块的输入；
  // Default、短边模式、8 对齐等尺寸规则统一由 Features.WindowControl 维护。
  return features::window_control::calculate_resolution_from_preset(
      ratio, preset, get_resolution_calculation_options(state, monitor_info));
}

// 变换前的准备
// 返回值：是否需要等待 overlay 首帧
auto prepare_transform_actions(core::AppState& state, HWND target_window, int target_width,
                               int target_height, const utils::display::MonitorInfo& monitor_info)
    -> bool {
  if (!state.overlay->enabled) {
    return false;
  }

  auto screen_w = utils::display::rect_width(monitor_info.monitor_rect);
  auto screen_h = utils::display::rect_height(monitor_info.monitor_rect);
  bool will_need_overlay = features::overlay::geometry::should_use_overlay(
      target_width, target_height, screen_w, screen_h);

  if (state.overlay->running.load(std::memory_order_acquire)) {
    // overlay 已运行，冻结当前帧
    state.overlay->is_transforming.store(true, std::memory_order_release);
    features::overlay::freeze_overlay(state);
    return false;  // 不需要等待首帧
  } else if (will_need_overlay) {
    // overlay 未运行，但目标尺寸需要 overlay，启动并在首帧后自动冻结
    state.overlay->is_transforming.store(true, std::memory_order_release);
    auto overlay_result = features::overlay::start_overlay(state, target_window, true);
    if (overlay_result) {
      return true;  // 需要等待首帧
    } else {
      Logger().error("Failed to start overlay before window transform: {}", overlay_result.error());
      state.overlay->is_transforming.store(false, std::memory_order_release);
      return false;
    }
  }

  // overlay 未运行，目标也不需要，什么都不做
  return false;
}

// 变换后的后续处理
auto post_transform_actions(core::AppState& state, HWND target_window,
                            const utils::display::MonitorInfo& monitor_info) -> void {
  if (state.overlay->is_transforming.load(std::memory_order_acquire)) {
    auto dimensions = features::overlay::geometry::get_window_dimensions(target_window);
    auto screen_w = utils::display::rect_width(monitor_info.monitor_rect);
    auto screen_h = utils::display::rect_height(monitor_info.monitor_rect);
    bool still_needs_overlay =
        dimensions && features::overlay::geometry::should_use_overlay(
                          dimensions->first, dimensions->second, screen_w, screen_h);

    // 先结束 transform 状态，再解冻 overlay。
    // 这样解冻后到达的第一帧新尺寸会被正常消费，而不会再走“变换中忽略 resize”的分支。
    state.overlay->is_transforming.store(false, std::memory_order_release);

    if (still_needs_overlay) {
      // 仍需 overlay：解冻继续
      features::overlay::unfreeze_overlay(state);
      features::overlay::interaction::suppress_taskbar_redraw(state);
    } else {
      // 不需要 overlay：停止
      features::overlay::stop_overlay(state);
    }
  }

  // 重启 letterbox
  if (!state.overlay->running.load(std::memory_order_acquire) && state.letterbox->enabled) {
    auto letterbox_result = features::letterbox::show(state, target_window);
    if (!letterbox_result) {
      Logger().error("Failed to restart letterbox after window transform: {}",
                     letterbox_result.error());
    }
  }
}

// 比例变换的完整协程流程
auto transform_ratio_async(core::AppState& state, size_t ratio_index, double ratio_value)
    -> core::async::ui_task {
  Logger().debug("[Coroutine] Transforming ratio to index {}, ratio: {}", ratio_index, ratio_value);

  // 查找目标窗口
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = features::window_control::find_target_window(window_title);
  if (!target_window) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    co_return;
  }

  const auto& fw = *state.floating_window;
  auto monitor_info = utils::display::get_working_monitor(fw.window.hwnd, fw.window.is_visible);
  if (!monitor_info) {
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + monitor_info.error());
    co_return;
  }

  // 计算目标分辨率
  auto new_resolution = calculate_menu_resolution(
      state, ratio_value, get_current_resolution_preset(state), *monitor_info);
  Logger().info("Window transform target (ratio change): {}x{}", new_resolution.width,
                new_resolution.height);

  // 准备变换
  bool needs_wait_first_frame = prepare_transform_actions(
      state, *target_window, new_resolution.width, new_resolution.height, *monitor_info);

  // 如果需要等待 overlay 首帧（最多 500ms）
  if (needs_wait_first_frame) {
    for (int i = 0; i < 50 && !state.overlay->freeze_rendering.load(std::memory_order_acquire);
         ++i) {
      co_await core::async::ui_delay{std::chrono::milliseconds(10)};
    }
  }

  // 应用窗口变换
  features::window_control::TransformOptions options{.activate_window = true};

  auto result = features::window_control::apply_window_transform(state, *target_window,
                                                                 new_resolution, options);
  if (!result) {
    state.overlay->is_transforming.store(false, std::memory_order_release);
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    co_return;
  }

  // 后续处理：等待窗口稳定后决定 overlay 状态
  if (state.overlay->is_transforming.load(std::memory_order_acquire)) {
    co_await core::async::ui_delay{std::chrono::milliseconds(400)};
    post_transform_actions(state, *target_window, *monitor_info);
  }

  // 更新当前比例索引
  const auto& ratios = features::settings::menu::get_ratios(state);
  if (ratio_index < ratios.size() || ratio_index == std::numeric_limits<size_t>::max()) {
    state.floating_window->ui.current_ratio_index = ratio_index;
  }

  // 请求重绘悬浮窗
  ui::floating_window::request_repaint(state);
}

// 处理比例改变事件（启动协程）
auto handle_ratio_changed(core::AppState& state,
                          const ui::floating_window::events::RatioChangeEvent& event) -> void {
  // 直接调用协程函数（ui_task 使用 suspend_never，立即开始执行）
  transform_ratio_async(state, event.index, event.ratio_value);
}

// 分辨率变换的完整协程流程
auto transform_resolution_async(core::AppState& state, size_t resolution_index)
    -> core::async::ui_task {
  Logger().debug("[Coroutine] Transforming resolution to index {}", resolution_index);

  // 查找目标窗口
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = features::window_control::find_target_window(window_title);
  if (!target_window) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    co_return;
  }

  const auto& fw = *state.floating_window;
  auto monitor_info = utils::display::get_working_monitor(fw.window.hwnd, fw.window.is_visible);
  if (!monitor_info) {
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + monitor_info.error());
    co_return;
  }

  // 计算目标分辨率
  double current_ratio = get_current_ratio(state, *monitor_info);
  auto new_resolution = calculate_menu_resolution(
      state, current_ratio, get_resolution_preset_by_index(state, resolution_index), *monitor_info);
  Logger().info("Window transform target (resolution change): {}x{}", new_resolution.width,
                new_resolution.height);

  // 准备变换
  bool needs_wait_first_frame = prepare_transform_actions(
      state, *target_window, new_resolution.width, new_resolution.height, *monitor_info);

  // 如果需要等待 overlay 首帧（最多 500ms）
  if (needs_wait_first_frame) {
    for (int i = 0; i < 50 && !state.overlay->freeze_rendering.load(std::memory_order_acquire);
         ++i) {
      co_await core::async::ui_delay{std::chrono::milliseconds(10)};
    }
  }

  // 应用窗口变换
  features::window_control::TransformOptions options{.activate_window = true};

  auto result = features::window_control::apply_window_transform(state, *target_window,
                                                                 new_resolution, options);
  if (!result) {
    state.overlay->is_transforming.store(false, std::memory_order_release);
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    co_return;
  }

  // 后续处理：等待窗口稳定后决定 overlay 状态
  if (state.overlay->is_transforming.load(std::memory_order_acquire)) {
    co_await core::async::ui_delay{std::chrono::milliseconds(400)};
    post_transform_actions(state, *target_window, *monitor_info);
  }

  // 更新当前分辨率索引
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  if (resolution_index < resolutions.size()) {
    state.floating_window->ui.current_resolution_index = resolution_index;
  }

  // 请求重绘悬浮窗
  ui::floating_window::request_repaint(state);
}

// 处理分辨率改变事件（启动协程）
auto handle_resolution_changed(core::AppState& state,
                               const ui::floating_window::events::ResolutionChangeEvent& event)
    -> void {
  // 直接调用协程函数（ui_task 使用 suspend_never，立即开始执行）
  transform_resolution_async(state, event.index);
}

// 处理窗口选择事件
auto handle_window_selected(core::AppState& state,
                            const ui::floating_window::events::WindowSelectionEvent& event)
    -> void {
  Logger().info("Window selected: {}", utils::string::ToUtf8(event.window_title));
  auto old_settings = state.settings->raw;

  // 更新设置状态中的目标窗口标题
  state.settings->raw.window.target_title = utils::string::ToUtf8(event.window_title);

  // 保存设置到文件
  bool did_persist_settings = false;
  auto settings_path = features::settings::get_settings_path();
  if (settings_path) {
    auto save_result =
        features::settings::save_settings_to_file(settings_path.value(), state.settings->raw);
    if (!save_result) {
      Logger().error("Failed to save settings: {}", save_result.error());
      // 可能需要通知用户保存失败
    } else {
      did_persist_settings = true;
    }
  } else {
    Logger().error("Failed to get settings path: {}", settings_path.error());
  }

  if (did_persist_settings) {
    features::settings::notify_settings_changed(state, old_settings,
                                                "Settings updated via window selection");
  }

  auto target_window = features::window_control::find_target_window(event.window_title);
  if (!target_window) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    return;
  }
  const auto& fw = *state.floating_window;
  auto monitor_info = utils::display::get_working_monitor(fw.window.hwnd, fw.window.is_visible);
  if (monitor_info) {
    post_transform_actions(state, target_window.value(), *monitor_info);
  }

  // 发送通知给用户
  core::notifications::show_notification(
      state, state.i18n->texts["label.app_name"],
      std::format("{}: {}", state.i18n->texts["message.window_selected"],
                  utils::string::ToUtf8(event.window_title)));
}

// 重置窗口变换（直接调用版本）
auto reset_window_transform(core::AppState& state) -> void {
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = features::window_control::find_target_window(window_title);
  if (!target_window) {
    Logger().error("Failed to find target window");
    return;
  }

  features::window_control::TransformOptions options{.activate_window = true};

  const auto& reset_resolution = state.settings->raw.window.reset_resolution;

  std::expected<void, std::string> result = std::unexpected("Unknown reset mode");
  if (reset_resolution.width > 0 && reset_resolution.height > 0) {
    features::window_control::Resolution resolution{
        .width = reset_resolution.width,
        .height = reset_resolution.height,
    };
    result = features::window_control::apply_window_transform(state, *target_window, resolution,
                                                              options);
  } else {
    result = features::window_control::reset_window_to_screen(state, *target_window, options);
  }

  if (!result) {
    Logger().error("Failed to reset window: {}", result.error());
    return;
  }

  // 重置后恢复浮窗选中状态：比例清空，分辨率回到 Default
  state.floating_window->ui.current_ratio_index = std::numeric_limits<size_t>::max();
  state.floating_window->ui.current_resolution_index = 0;
  ui::floating_window::request_repaint(state);
}

}  // namespace features::window_control
