module;

module Handlers.Window;

import std;
import Common.MenuData;
import Core.Events;
import Core.WebView.Events;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import Core.State;
import Core.I18n.State;
import Features.Settings;
import Features.Settings.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Preview.Window;
import Features.Overlay;
import UI.AppWindow;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;

namespace Handlers {

// 获取当前比例
auto get_current_ratio(const Core::State::AppState& state) -> double {
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
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
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  if (state.app_window->ui.current_resolution_index < resolutions.size()) {
    return resolutions[state.app_window->ui.current_resolution_index].totalPixels;
  }
  return 0;  // 表示使用屏幕尺寸
}

// 在窗口变换前停止活动模块
auto prepare_window_transform(Core::State::AppState& state) -> void {
  // 停止活动模块
  if (state.app_window->ui.preview_enabled) {
    Features::Preview::Window::stop_preview(state);
  }

  if (state.app_window->ui.overlay_enabled) {
    Features::Overlay::stop_overlay(state);
  }
}

// 变换后的后续处理
auto post_transform_actions(Core::State::AppState& state, Vendor::Windows::HWND target_window)
    -> void {
  // 重启之前启用的模块
  if (state.app_window->ui.preview_enabled) {
    auto preview_result = Features::Preview::Window::start_preview(state, target_window);
    if (!preview_result) {
      Logger().error("Failed to restart preview after window transform: {}",
                     preview_result.error());
    }
  }

  if (state.app_window->ui.overlay_enabled) {
    auto overlay_result = Features::Overlay::start_overlay(state, target_window);
    if (!overlay_result) {
      Logger().error("Failed to restart overlay after window transform: {}",
                     overlay_result.error());
    }
  }

  Logger().debug("Post-transform actions completed");
}



// 处理重置窗口事件
auto handle_reset_event(Core::State::AppState& state,
                        const UI::AppWindow::Events::ResetEvent& event) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

  prepare_window_transform(state);

  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->config.window.taskbar.lower_on_resize,
      .activate_window = true};

  auto result = Features::WindowControl::reset_window_to_screen(*target_window, options);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts.label.app_name,
        state.i18n->texts.message.window_reset_failed + ": " + result.error());
    return;
  }

  post_transform_actions(state, *target_window);

  // 重置UI状态到默认值
  state.app_window->ui.current_ratio_index =
      std::numeric_limits<size_t>::max();  // 表示使用屏幕比例
  state.app_window->ui.current_resolution_index = 0;

  // 触发UI更新
  if (state.app_window->window.hwnd) {
    Vendor::Windows::InvalidateRect(state.app_window->window.hwnd, nullptr, true);
  }

  Logger().debug("Window reset to screen size successfully");
}

// 处理比例改变事件
auto handle_ratio_changed(Core::State::AppState& state,
                          const UI::AppWindow::Events::RatioChangeEvent& event) -> void {
  Logger().debug("Handling ratio change to index {}, ratio: {}", event.index, event.ratio_value);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

  // 计算新分辨率
  auto total_pixels = get_current_total_pixels(state);
  Features::WindowControl::Resolution new_resolution;

  if (total_pixels == 0) {
    // 使用屏幕尺寸计算
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(event.ratio_value);
  } else {
    new_resolution = Features::WindowControl::calculate_resolution(event.ratio_value, total_pixels);
  }

  prepare_window_transform(state);

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->config.window.taskbar.lower_on_resize,
      .activate_window = !state.app_window->ui.overlay_enabled};

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
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
  if (event.index < ratios.size() || event.index == std::numeric_limits<size_t>::max()) {
    state.app_window->ui.current_ratio_index = event.index;
  }

  // 触发UI更新
  if (state.app_window->window.hwnd) {
    Vendor::Windows::InvalidateRect(state.app_window->window.hwnd, nullptr, true);
  }
}

// 处理分辨率改变事件
auto handle_resolution_changed(Core::State::AppState& state,
                               const UI::AppWindow::Events::ResolutionChangeEvent& event) -> void {
  Logger().debug("Handling resolution change to index {}, pixels: {}", event.index,
                 event.total_pixels);

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->config.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts.label.app_name,
                                               state.i18n->texts.message.window_not_found);
    return;
  }

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

  prepare_window_transform(state);

  // 应用窗口变换
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = state.settings->config.window.taskbar.lower_on_resize,
      .activate_window = !state.app_window->ui.overlay_enabled};

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
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  if (event.index < resolutions.size()) {
    state.app_window->ui.current_resolution_index = event.index;
  }

  // 触发UI更新
  if (state.app_window->window.hwnd) {
    Vendor::Windows::InvalidateRect(state.app_window->window.hwnd, nullptr, true);
  }
}

// 处理窗口选择事件
auto handle_window_selected(Core::State::AppState& state,
                            const UI::AppWindow::Events::WindowSelectionEvent& event) -> void {
  Logger().info("Window selected: {}", Utils::String::ToUtf8(event.window_title));

  // 更新设置状态中的目标窗口标题
  state.settings->config.window.target_title = Utils::String::ToUtf8(event.window_title);

  // 保存设置到文件
  auto settings_path = Features::Settings::get_settings_path();
  if (settings_path) {
    auto save_result =
        Features::Settings::save_settings_to_file(settings_path.value(), state.settings->config);
    if (!save_result) {
      Logger().error("Failed to save settings: {}", save_result.error());
      // 可能需要通知用户保存失败
    }
  } else {
    Logger().error("Failed to get settings path: {}", settings_path.error());
  }

  // 重启模块
  prepare_window_transform(state);
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

// 注册窗口处理器
auto register_window_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 处理比例改变
  subscribe<UI::AppWindow::Events::RatioChangeEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::RatioChangeEvent& event) {
        handle_ratio_changed(app_state, event);
      });

  // 处理分辨率改变
  subscribe<UI::AppWindow::Events::ResolutionChangeEvent>(
      *app_state.event_bus,
      [&app_state](const UI::AppWindow::Events::ResolutionChangeEvent& event) {
        handle_resolution_changed(app_state, event);
      });

  subscribe<UI::AppWindow::Events::ResetEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::ResetEvent& event) {
        handle_reset_event(app_state, event);
      });

  // 处理窗口选择
  subscribe<UI::AppWindow::Events::WindowSelectionEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::WindowSelectionEvent& event) {
        handle_window_selected(app_state, event);
      });
}

}  // namespace Handlers