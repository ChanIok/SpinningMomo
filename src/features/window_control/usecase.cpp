module;

module Features.WindowControl.UseCase;

import std;
import Features.Settings.Menu;
import Core.State;
import Core.I18n.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import UI.AppWindow.State;
import Features.Settings;
import Features.Settings.State;
import Features.Letterbox;
import Features.WindowControl;
import Features.Notifications;
import Features.Overlay;
import Features.Overlay.State;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;

namespace Features::WindowControl::UseCase {

// 获取当前比例
auto get_current_ratio(const Core::State::AppState& state) -> double {
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  if (state.app_window->ui.current_ratio_index < ratios.size()) {
    return ratios[state.app_window->ui.current_ratio_index].ratio;
  }

  // 默认使用屏幕比例
  int screen_width = Vendor::Windows::GetScreenWidth();
  int screen_height = Vendor::Windows::GetScreenHeight();
  return static_cast<double>(screen_width) / screen_height;
}

// 获取当前总像素数
auto get_current_total_pixels(const Core::State::AppState& state) -> std::uint64_t {
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  if (state.app_window->ui.current_resolution_index < resolutions.size()) {
    return resolutions[state.app_window->ui.current_resolution_index].total_pixels;
  }
  return 0;  // 表示使用屏幕尺寸
}

// 变换前的准备
auto prepare_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window)
    -> void {
  // 提取启动overlay
  if (state.app_window->ui.overlay_enabled) {
    Logger().debug("Starting overlay before window transform");
    auto overlay_result = Features::Overlay::start_overlay(state, target_window, true);
    if (!overlay_result) {
      Logger().error("Failed to start overlay before window transform: {}", overlay_result.error());
    }
  }
}

// 变换后的后续处理
auto post_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window)
    -> void {
  // 重启letterbox
  if (!state.overlay->running && state.app_window->ui.letterbox_enabled) {
    auto letterbox_result = Features::Letterbox::show(state, target_window);
    if (!letterbox_result) {
      Logger().error("Failed to restart letterbox after window transform: {}",
                     letterbox_result.error());
    }
  }
  Logger().debug("Post-transform actions completed");
}

// 处理比例改变事件
auto handle_ratio_changed(Core::State::AppState& state,
                          const UI::AppWindow::Events::RatioChangeEvent& event) -> void {
  Logger().debug("Handling ratio change to index {}, ratio: {}", event.index, event.ratio_value);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

  prepare_transform_actions(state, target_window.value());

  // 计算新分辨率
  auto total_pixels = get_current_total_pixels(state);
  Features::WindowControl::Resolution new_resolution;

  if (total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(event.ratio_value);
  } else {
    new_resolution = Features::WindowControl::calculate_resolution(event.ratio_value, total_pixels);
  }

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->raw.window.taskbar.lower_on_resize, .activate_window = true};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts.label.app_name,
        state.i18n->texts.message.window_adjust_failed + ": " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, target_window.value());

  // 更新当前比例索引
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  if (event.index < ratios.size() || event.index == std::numeric_limits<size_t>::max()) {
    state.app_window->ui.current_ratio_index = event.index;
  }
}

// 处理分辨率改变事件
auto handle_resolution_changed(Core::State::AppState& state,
                               const UI::AppWindow::Events::ResolutionChangeEvent& event) -> void {
  Logger().debug("Handling resolution change to index {}, pixels: {}", event.index,
                 event.total_pixels);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

  prepare_transform_actions(state, target_window.value());

  // 计算新分辨率
  double current_ratio = get_current_ratio(state);
  Features::WindowControl::Resolution new_resolution;

  if (event.total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(current_ratio);
  } else {
    new_resolution =
        Features::WindowControl::calculate_resolution(current_ratio, event.total_pixels);
  }

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->raw.window.taskbar.lower_on_resize, .activate_window = true};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts.label.app_name,
        state.i18n->texts.message.window_adjust_failed + ": " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, target_window.value());

  // 更新当前分辨率索引
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  if (event.index < resolutions.size()) {
    state.app_window->ui.current_resolution_index = event.index;
  }
}

// 处理窗口选择事件
auto handle_window_selected(Core::State::AppState& state,
                            const UI::AppWindow::Events::WindowSelectionEvent& event) -> void {
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
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }
  post_transform_actions(state, target_window.value());

  // 发送通知给用户
  Features::Notifications::show_notification(
      state, state.i18n->texts.label.app_name,
      std::format("{}: {}", state.i18n->texts.message.window_selected,
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
