#include "features/screenshot/usecase.hpp"

#include "vendor/std.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "features/adb_mode/usecase.hpp"
#include "features/photography/state.hpp"
#include "features/screenshot/screenshot.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/floating_window/events.hpp"
#include "utils/image/image.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"
#include "utils/system/system.hpp"

namespace features::screenshot {

// 响应通知栏查看动作：根据配置在资源管理器定位文件或使用系统默认关联程序打开
auto handle_saved_file_view_action(core::AppState& state, const std::filesystem::path& path,
                                   std::string_view file_kind) -> void {
  const auto& action = state.settings->raw.features.saved_file_view_action;
  // 根据用户偏好设置选择在 Explorer 中定位还是直接打开
  auto action_result = action == "reveal_in_explorer"
                           ? utils::system::reveal_file_in_explorer(path)
                           : utils::system::open_file_with_default_app(path);
  if (!action_result) {
    Logger().warn("Failed to handle {} view action '{}': {}", file_kind, action,
                  action_result.error());
  }
}

// 执行 ADB 模式截图：算输出目录 → 取格式配置 → 生成时间戳路径 → 发起异步截屏并挂完成通知
auto capture_adb(core::AppState& state) -> void {
  // 检查是否启用了按窗口标题分类子目录存储
  std::optional<std::filesystem::path> output_dir_override;
  if (state.settings->raw.features.organize_output_by_window_title) {
    auto output_dir_result = utils::path::GetOutputDirectoryForWindowTitle(
        state.settings->raw.features.output_dir_path, L"ADB");
    if (!output_dir_result) {
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.screenshot_failed"] + ": " + output_dir_result.error());
      Logger().error("Failed to resolve ADB screenshot output directory: {}",
                     output_dir_result.error());
      return;
    }
    output_dir_override = *output_dir_result;
  }

  // 获取通用输出目录路径
  std::filesystem::path screenshots_dir;
  if (output_dir_override) {
    screenshots_dir = *output_dir_override;
  } else {
    auto output_dir_result =
        utils::path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
    if (!output_dir_result) {
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.screenshot_failed"] + ": " + output_dir_result.error());
      Logger().error("Failed to resolve ADB screenshot output directory: {}",
                     output_dir_result.error());
      return;
    }
    screenshots_dir = output_dir_result.value();
  }

  // 根据当前配置决定输出为 PNG 还是 JPEG
  auto adb_format = features::adb_mode::AdbScreenshotFormat::PNG;
  const auto& configured_format = state.settings->raw.features.screenshot.file_format;
  if (configured_format == "jpeg" || configured_format == "jpg") {
    adb_format = features::adb_mode::AdbScreenshotFormat::JPEG;
  }

  // 格式化当前系统时间戳作为截图文件名
  auto file_path =
      screenshots_dir /
      std::filesystem::path(utils::string::FormatTimestamp(std::chrono::system_clock::now()));
  if (adb_format == features::adb_mode::AdbScreenshotFormat::JPEG) {
    file_path.replace_extension(L".jpg");
  }

  // 构造异步截图完成回调：成功发出系统交互通知，失败上报错误提示
  auto completion_callback = [&state](bool success, const std::wstring& path, std::string error) {
    if (success) {
      const std::filesystem::path screenshot_path(path);
      core::notifications::NotificationOptions options;
      options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
      options.message =
          utils::string::FromUtf8(state.i18n->texts["message.screenshot_success"]) + path;

      // 绑定通知弹窗的“查看”按钮回调
      core::notifications::NotificationAction view_action;
      view_action.label = utils::string::FromUtf8(state.i18n->texts["notification.action.view"]);
      view_action.callback = [screenshot_path](core::AppState& app_state) {
        handle_saved_file_view_action(app_state, screenshot_path, "screenshot");
      };
      options.action = std::move(view_action);
      core::notifications::post_notification_request(state, std::move(options));
      Logger().info("ADB screenshot saved successfully: {}", utils::string::ToUtf8(path));
      return;
    }

    // 截图失败发出告警通知
    core::notifications::NotificationOptions options;
    options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
    options.message = utils::string::FromUtf8(state.i18n->texts["message.screenshot_failed"]);
    core::notifications::post_notification_request(state, std::move(options));
    Logger().error("ADB screenshot failed: {}; reason: {}", utils::string::ToUtf8(path), error);
  };

  // 向 ADB 任务队列投递截屏操作；未连接时提示错误
  if (!features::adb_mode::capture_screen_async(state, file_path, adb_format,
                                                std::move(completion_callback))) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.screenshot_failed"] + ": " +
                                               state.i18n->texts["message.adb_not_connected"]);
  }
}

// 统一截图入口：检查 ADB 状态分流 → 查找目标游戏窗口 → 解析目录与格式 → 启动截图管道
auto capture(core::AppState& state) -> void {
  // ADB 模式已连接时，优先走 ADB 专用通道采集模拟器画面
  if (features::adb_mode::is_connected(state)) {
    capture_adb(state);
    return;
  }

  // 查找配置的目标游戏窗口句柄
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = features::window_control::find_target_window(window_title);
  if (!target_window) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 按窗口标题分目录存储截图
  std::optional<std::filesystem::path> output_dir_override;
  if (state.settings->raw.features.organize_output_by_window_title) {
    auto actual_title = features::window_control::get_window_title(*target_window);
    if (!actual_title) {
      Logger().warn("Failed to read current screenshot target title, using configured title: {}",
                    actual_title.error());
    }

    const auto title = actual_title.value_or(window_title);
    auto output_dir_result = utils::path::GetOutputDirectoryForWindowTitle(
        state.settings->raw.features.output_dir_path, title);
    if (!output_dir_result) {
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.screenshot_failed"] + ": " + output_dir_result.error());
      Logger().error("Failed to resolve screenshot output directory: {}",
                     output_dir_result.error());
      return;
    }
    output_dir_override = *output_dir_result;
  }

  // 截图完成回调在截图工作线程的帧回调中执行，必须快速返回；通知通过事件系统发送到 UI 线程
  auto completion_callback = [&state](bool success, const std::wstring& path) {
    if (success) {
      const std::filesystem::path screenshot_path(path);
      const auto path_str = utils::string::ToUtf8(path);

      core::notifications::NotificationOptions options;
      options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
      options.message =
          utils::string::FromUtf8(state.i18n->texts["message.screenshot_success"]) + path;

      // 绑定查看动作回调
      core::notifications::NotificationAction view_action;
      view_action.label = utils::string::FromUtf8(state.i18n->texts["notification.action.view"]);
      view_action.callback = [screenshot_path](core::AppState& app_state) {
        handle_saved_file_view_action(app_state, screenshot_path, "screenshot");
      };
      options.action = std::move(view_action);

      core::notifications::post_notification_request(state, std::move(options));
      Logger().info("Screenshot saved successfully: {}", path_str);
    } else {
      core::notifications::NotificationOptions fail_options;
      fail_options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
      fail_options.message =
          utils::string::FromUtf8(state.i18n->texts["message.screenshot_failed"]);
      core::notifications::post_notification_request(state, std::move(fail_options));
      Logger().error("Screenshot capture failed");
    }
  };

  // 读取目标图片输出格式与质量参数
  utils::image::ImageFormat image_format = utils::image::ImageFormat::PNG;
  const auto& fmt = state.settings->raw.features.screenshot.file_format;
  if (fmt == "jpeg" || fmt == "jpg") {
    image_format = utils::image::ImageFormat::JPEG;
  }
  float jpeg_quality = 1.0f;

  // 若高级摄影模式开启，将帧数传入截图管道以启用长曝光累积
  int shutter_frames = 0;
  if (state.photography->enabled.load(std::memory_order_acquire)) {
    shutter_frames = std::max(0, state.photography->shutter_frames.load(std::memory_order_acquire));
  }

  const auto capture_client_area = state.settings->raw.features.screenshot.capture_client_area;

  // 调用 Windows 屏幕捕获管道抓取窗口画面
  auto result = features::screenshot::take_screenshot(
      state, *target_window, std::move(completion_callback), image_format, jpeg_quality,
      output_dir_override, shutter_frames, capture_client_area);
  if (!result) {
    core::notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.screenshot_failed"] + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 响应热键或快捷事件：忽略事件负载并直接转调主截图函数
auto handle_capture_event(core::AppState& state,
                          const ui::floating_window::events::CaptureEvent& event) -> void {
  static_cast<void>(event);
  capture(state);
}

}  // namespace features::screenshot
