module;

module Features.Screenshot.UseCase;

import std;
import Core.State;
import Core.Events;
import Core.I18n.State;
import Core.WorkerPool;
import UI.FloatingWindow.Events;
import Features.Screenshot;
import Features.Settings.State;
import Features.ReplayBuffer.UseCase;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.String;

namespace Features::Screenshot::UseCase {

// 截图
auto capture(Core::State::AppState& state) -> void {
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (!target_window) {
    Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               state.i18n->texts["message.window_not_found"]);
    return;
  }

  // 检查是否需要生成 Motion Photo
  bool motion_photo_enabled =
      state.replay_buffer &&
      state.replay_buffer->motion_photo_enabled.load(std::memory_order_acquire) &&
      state.replay_buffer->status.load(std::memory_order_acquire) ==
          Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering;

  // 创建截图完成回调
  // 注意：这个回调在截图工作线程的帧回调中执行，必须快速返回
  // Motion Photo 处理提交到 WorkerPool 异步执行，通知通过事件系统发送到 UI 线程
  auto completion_callback = [&state, motion_photo_enabled](bool success,
                                                            const std::wstring& path) {
    if (success) {
      std::string path_str(path.begin(), path.end());

      // 如果启用了 Motion Photo，提交到 WorkerPool 异步处理
      if (motion_photo_enabled) {
        // 捕获必要的数据（避免捕获引用导致生命周期问题）
        std::filesystem::path jpeg_path_copy(path);
        std::string app_name = state.i18n->texts["label.app_name"];
        std::string success_msg = state.i18n->texts["message.screenshot_success"];

        bool submitted = Core::WorkerPool::submit_task(
            *state.worker_pool,
            [&state, jpeg_path_copy = std::move(jpeg_path_copy), app_name = std::move(app_name),
             success_msg = std::move(success_msg)]() {
              auto mp_result =
                  Features::ReplayBuffer::UseCase::save_motion_photo(state, jpeg_path_copy);

              // 通过事件系统发送通知（跨线程安全）
              if (mp_result) {
                auto mp_path_str = mp_result->string();
                Core::Events::post(*state.events,
                                   UI::FloatingWindow::Events::NotificationEvent{
                                       .title = app_name, .message = success_msg + mp_path_str});
                Logger().debug("Motion Photo saved: {}", mp_path_str);
              } else {
                Logger().error("Motion Photo creation failed: {}", mp_result.error());
                // 回退到普通截图通知（显示原始 JPEG 路径）
                Core::Events::post(
                    *state.events,
                    UI::FloatingWindow::Events::NotificationEvent{
                        .title = app_name, .message = success_msg + jpeg_path_copy.string()});
              }
            });

        if (!submitted) {
          Logger().error("Failed to submit Motion Photo task to worker pool");
          // WorkerPool 提交失败，回退到普通截图通知
          Core::Events::post(
              *state.events,
              UI::FloatingWindow::Events::NotificationEvent{
                  .title = state.i18n->texts["label.app_name"],
                  .message = state.i18n->texts["message.screenshot_success"] + path_str});
        }
      } else {
        // 普通截图模式：通过事件系统发送通知
        Core::Events::post(
            *state.events,
            UI::FloatingWindow::Events::NotificationEvent{
                .title = state.i18n->texts["label.app_name"],
                .message = state.i18n->texts["message.screenshot_success"] + path_str});
        Logger().debug("Screenshot saved successfully: {}", path_str);
      }
    } else {
      // 截图失败通知
      Core::Events::post(*state.events,
                         UI::FloatingWindow::Events::NotificationEvent{
                             .title = state.i18n->texts["label.app_name"],
                             .message = state.i18n->texts["message.window_adjust_failed"]});
      Logger().error("Screenshot capture failed");
    }
  };

  // 执行截图（Motion Photo 模式使用 JPEG 格式）
  auto image_format =
      motion_photo_enabled ? Utils::Image::ImageFormat::JPEG : Utils::Image::ImageFormat::PNG;
  float jpeg_quality = 1.0f;  // 视觉无损

  std::optional<std::filesystem::path> output_dir_override;
  if (motion_photo_enabled) {
    auto exe_dir_result = Utils::Path::GetExecutableDirectory();
    if (exe_dir_result) {
      auto temp_dir = *exe_dir_result / "cache" / "motion_photo_temp";
      auto ensure_result = Utils::Path::EnsureDirectoryExists(temp_dir);
      if (ensure_result) {
        output_dir_override = temp_dir;
      }
    }
  }

  auto result = Features::Screenshot::take_screenshot(
      state, *target_window, completion_callback, image_format, jpeg_quality, output_dir_override);
  if (!result) {
    Features::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.window_adjust_failed"] + ": " + result.error());
    Logger().error("Failed to start screenshot: {}", result.error());
  } else {
    Logger().debug("Screenshot capture started successfully");
  }
}

// 处理截图事件（Event版本，用于热键系统）
auto handle_capture_event(Core::State::AppState& state,
                          const UI::FloatingWindow::Events::CaptureEvent& event) -> void {
  capture(state);
}

}  // namespace Features::Screenshot::UseCase
