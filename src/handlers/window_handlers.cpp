module;

module Handlers.Window;

import std;
import Common.MenuData;
import Core.Events;
import Core.WebView.Events;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import Core.State;
import Features.WindowControl;
import Features.Notifications;
import Features.Screenshot;
import Features.Screenshot.Folder;
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

// 处理截图事件
auto handle_capture_event(Core::State::AppState& state,
                          const UI::AppWindow::Events::CaptureEvent& event) -> void {
  // TODO: 等待settings设计完成后，从settings中读取窗口标题
  std::wstring window_title = L"";
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    return;
  }

  // 创建截图完成回调
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      // 转换路径为字符串用于通知
      std::string path_str(path.begin(), path.end());
      Features::Notifications::show_notification(state, "SpinningMomo",
                                                 "Screenshot saved to: " + path_str);
      Logger().debug("Screenshot saved successfully: {}", path_str);
    } else {
      Features::Notifications::show_notification(state, "SpinningMomo",
                                                 "Failed to capture screenshot");
      Logger().error("Screenshot capture failed");
    }
  };

  // 执行截图
  auto result =
      Features::Screenshot::take_screenshot(*state.screenshot, *target_window, completion_callback);
  if (!result) {
    Features::Notifications::show_notification(state, "SpinningMomo",
                                               "Failed to start screenshot: " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理打开截图文件夹事件
auto handle_screenshots_event(Core::State::AppState& state,
                              const UI::AppWindow::Events::ScreenshotsEvent& event) -> void {
  Logger().debug("Opening screenshot folder");

  if (auto result = Features::Screenshot::Folder::open_folder(state); !result) {
    Logger().error("Failed to open screenshot folder: {}", result.error());
    Features::Notifications::show_notification(state, "SpinningMomo",
                                               "Failed to open screenshot folder");
  }
}

// 处理重置窗口事件
auto handle_reset_event(Core::State::AppState& state,
                        const UI::AppWindow::Events::ResetEvent& event) -> void {
  // TODO: 等待settings设计完成后，从settings中读取窗口标题
  std::wstring window_title = L"";
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    return;
  }

  // TODO: 等待settings设计完成后，从settings中读取taskbar.lower配置
  Features::WindowControl::TransformOptions options{.taskbar_lower = false,
                                                    .activate_window = true};

  auto result = Features::WindowControl::reset_window_to_screen(*target_window, options);
  if (!result) {
    Features::Notifications::show_notification(state, "SpinningMomo",
                                               "Failed to reset window: " + result.error());
    return;
  }

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
  // TODO: 等待settings设计完成后，从settings中读取窗口标题
  std::wstring window_title = L"";
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
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

  // 应用窗口变换
  // TODO: 等待settings设计完成后，从settings中读取taskbar.lower配置
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = false, .activate_window = !state.app_window->ui.overlay_enabled};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Failed to apply window transform: " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, *target_window, new_resolution);

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
  // TODO: 等待settings设计完成后，从settings中读取窗口标题
  std::wstring window_title = L"";
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Target window not found. Please ensure the game is running.");
    return;
  }

  // 获取当前比例
  double current_ratio = get_current_ratio(state);

  // 计算新分辨率
  Features::WindowControl::Resolution new_resolution;
  if (event.total_pixels == 0) {
    // 默认选项，使用屏幕尺寸
    new_resolution = Features::WindowControl::calculate_resolution_by_screen(current_ratio);
  } else {
    new_resolution =
        Features::WindowControl::calculate_resolution(current_ratio, event.total_pixels);
  }

  // 应用窗口变换
  // TODO: 等待settings设计完成后，从settings中读取taskbar.lower配置
  Features::WindowControl::TransformOptions options{
      .taskbar_lower = false, .activate_window = !state.app_window->ui.overlay_enabled};

  auto result =
      Features::WindowControl::apply_window_transform(*target_window, new_resolution, options);
  if (!result) {
    Features::Notifications::show_notification(
        state, "SpinningMomo", "Failed to apply window transform: " + result.error());
    return;
  }

  Logger().debug("Window transform applied successfully: {}x{}", new_resolution.width,
                 new_resolution.height);

  post_transform_actions(state, *target_window, new_resolution);

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

  // 这里可以添加窗口选择的业务逻辑
  // 例如：
  // 1. 更新应用状态中的目标窗口
  // 2. 保存到配置文件
  // 3. 触发其他相关功能（如开始预览、更新UI状态等）

  // 示例：更新目标窗口信息（如果有相关状态管理）
  // state.settings->window.target_title = event.window_title;

  // 示例：可能触发其他业务逻辑
  // 如果启用了预览功能，可能需要重新开始预览新窗口
  if (state.app_window->ui.preview_enabled) {
    Logger().debug("Preview is enabled, may need to restart preview for new target window");
    // Features::Preview::restart_with_target(state, event.window_handle);
  }

  // 发送通知给用户
  Features::Notifications::show_notification(
      state, "SpinningMomo",
      std::format("已选择窗口: {}", Utils::String::ToUtf8(event.window_title)));
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

  // 处理窗口动作事件
  subscribe<UI::AppWindow::Events::CaptureEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::CaptureEvent& event) {
        handle_capture_event(app_state, event);
      });

  subscribe<UI::AppWindow::Events::ScreenshotsEvent>(
      *app_state.event_bus, [&app_state](const UI::AppWindow::Events::ScreenshotsEvent& event) {
        handle_screenshots_event(app_state, event);
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