module;

module Features.WindowControl.UseCase;

import std;
import Features.Settings.Menu;
import Core.State;
import Core.I18n.State;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import UI.FloatingWindow.State;
import Features.Settings;
import Features.Settings.State;
import Features.Letterbox;
import Features.Letterbox.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay;
import Features.Overlay.State;
import Features.Overlay.Geometry;
import Features.Overlay.Interaction;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;

namespace Features::WindowControl::UseCase {

// 获取当前比例
auto get_current_ratio(const Core::State::AppState& state) -> double {
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  if (state.floating_window->ui.current_ratio_index < ratios.size()) {
    return ratios[state.floating_window->ui.current_ratio_index].ratio;
  }

  // 默认使用屏幕比例
  int screen_width = Vendor::Windows::GetScreenWidth();
  int screen_height = Vendor::Windows::GetScreenHeight();
  return static_cast<double>(screen_width) / screen_height;
}

// 获取当前总像素数
auto get_current_total_pixels(const Core::State::AppState& state) -> std::uint64_t {
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  if (state.floating_window->ui.current_resolution_index < resolutions.size()) {
    return resolutions[state.floating_window->ui.current_resolution_index].total_pixels;
  }
  return 0;  // 表示使用屏幕尺寸
}

// 变换前的准备
auto prepare_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window,
                               int target_width, int target_height) -> void {
  if (!state.overlay->enabled) {
    return;
  }

  auto [screen_w, screen_h] = Features::Overlay::Geometry::get_screen_dimensions();
  bool will_need_overlay = Features::Overlay::Geometry::should_use_overlay(
      target_width, target_height, screen_w, screen_h);

  if (state.overlay->running) {
    // overlay 已运行，冻结当前帧
    state.overlay->is_transforming = true;
    Features::Overlay::freeze_overlay(state);
  } else if (will_need_overlay) {
    // overlay 未运行，但目标尺寸需要 overlay，启动并在首帧后自动冻结
    state.overlay->is_transforming = true;
    auto overlay_result = Features::Overlay::start_overlay(state, target_window, true);
    if (overlay_result) {
      // 等待首帧渲染完成并冻结（最多等待 500ms）
      for (int i = 0; i < 50 && !state.overlay->freeze_rendering; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    } else {
      Logger().error("Failed to start overlay before window transform: {}", overlay_result.error());
      state.overlay->is_transforming = false;
    }
  }
  // 否则：overlay 未运行，目标也不需要，什么都不做
}

// 变换后的后续处理
auto post_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window)
    -> void {
  // 处理 overlay 变换后的状态
  if (state.overlay->is_transforming) {
    // 等待窗口调整稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // 获取新窗口尺寸，决定是否继续 overlay
    auto dimensions = Features::Overlay::Geometry::get_window_dimensions(target_window);
    auto [screen_w, screen_h] = Features::Overlay::Geometry::get_screen_dimensions();

    if (dimensions && Features::Overlay::Geometry::should_use_overlay(
                          dimensions->first, dimensions->second, screen_w, screen_h)) {
      // 仍需 overlay：解冻继续
      Features::Overlay::unfreeze_overlay(state);
      // 主动禁止任务栏重绘，防止闪烁
      Features::Overlay::Interaction::suppress_taskbar_redraw(state);
    } else {
      // 不需要 overlay：停止
      Features::Overlay::stop_overlay(state);
    }

    state.overlay->is_transforming = false;
  }

  // 重启letterbox
  if (!state.overlay->running && state.letterbox->enabled) {
    auto letterbox_result = Features::Letterbox::show(state, target_window);
    if (!letterbox_result) {
      Logger().error("Failed to restart letterbox after window transform: {}",
                     letterbox_result.error());
    }
  }
}

// 处理比例改变事件
auto handle_ratio_changed(Core::State::AppState& state,
                          const UI::FloatingWindow::Events::RatioChangeEvent& event) -> void {
  Logger().debug("Handling ratio change to index {}, ratio: {}", event.index, event.ratio_value);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 先计算目标分辨率
  auto total_pixels = get_current_total_pixels(state);
  Features::WindowControl::Resolution new_resolution;

  if (total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(event.ratio_value);
  } else {
    new_resolution = Features::WindowControl::calculate_resolution(event.ratio_value, total_pixels);
  }

  // 准备变换（根据目标尺寸决定是否启动 overlay）
  prepare_transform_actions(state, target_window.value(), new_resolution.width,
                            new_resolution.height);

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->raw.window.taskbar.lower_on_resize, .activate_window = true};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    state.overlay->is_transforming = false;
    Features::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    return;
  }

  post_transform_actions(state, target_window.value());

  // 更新当前比例索引
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  if (event.index < ratios.size() || event.index == std::numeric_limits<size_t>::max()) {
    state.floating_window->ui.current_ratio_index = event.index;
  }
}

// 处理分辨率改变事件
auto handle_resolution_changed(Core::State::AppState& state,
                               const UI::FloatingWindow::Events::ResolutionChangeEvent& event)
    -> void {
  Logger().debug("Handling resolution change to index {}, pixels: {}", event.index,
                 event.total_pixels);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 先计算目标分辨率
  double current_ratio = get_current_ratio(state);
  Features::WindowControl::Resolution new_resolution;

  if (event.total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(current_ratio);
  } else {
    new_resolution =
        Features::WindowControl::calculate_resolution(current_ratio, event.total_pixels);
  }

  // 准备变换（根据目标尺寸决定是否启动 overlay）
  prepare_transform_actions(state, target_window.value(), new_resolution.width,
                            new_resolution.height);

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->raw.window.taskbar.lower_on_resize, .activate_window = true};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    state.overlay->is_transforming = false;
    Features::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    return;
  }

  post_transform_actions(state, target_window.value());

  // 更新当前分辨率索引
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  if (event.index < resolutions.size()) {
    state.floating_window->ui.current_resolution_index = event.index;
  }
}

// 处理窗口选择事件
auto handle_window_selected(Core::State::AppState& state,
                            const UI::FloatingWindow::Events::WindowSelectionEvent& event) -> void {
  Logger().info("Window selected: {}", Utils::String::ToUtf8(event.window_title));

  // 更新设置状态中的目标窗口标题
  state.settings->raw.window.target_title = Utils::String::ToUtf8(event.window_title);

  // 保存设置到文件
  auto settings_path = Features::Settings::get_settings_path();
  if (settings_path) {
    auto save_result =
        Features::Settings::save_settings_to_file(settings_path.value(), state.settings->raw);
    if (!save_result) {
      Logger().error("Failed to save settings: {}", save_result.error());
      // 可能需要通知用户保存失败
    }
  } else {
    Logger().error("Failed to get settings path: {}", settings_path.error());
  }

  auto target_window = Features::WindowControl::find_target_window(event.window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               state.i18n->texts["message.window_not_found"]);
    return;
  }
  post_transform_actions(state, target_window.value());

  // 发送通知给用户
  Features::Notifications::show_notification(
      state, state.i18n->texts["label.app_name"],
      std::format("{}: {}", state.i18n->texts["message.window_selected"],
                  Utils::String::ToUtf8(event.window_title)));
}

// 重置窗口变换（直接调用版本）
auto reset_window_transform(Core::State::AppState& state) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Logger().error("Failed to find target window");
    return;
  }

  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->raw.window.taskbar.lower_on_resize, .activate_window = true};

  auto result = Features::WindowControl::reset_window_to_screen(*target_window, options);
  if (!result) {
    Logger().error("Failed to reset window: {}", result.error());
  }
}

}  // namespace Features::WindowControl::UseCase
