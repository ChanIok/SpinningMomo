module;

module Features.Screenshot.UseCase;

import std;
import Core.State;
import Core.I18n.State;
import UI.FloatingWindow.Events;
import Features.Screenshot;
import Features.Screenshot.Folder;
import Features.Settings.State;
import Features.ReplayBuffer.UseCase;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import Features.WindowControl;
import Features.Notifications;
import Utils.Image;
import Utils.Logger;
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
  bool motion_photo_enabled = state.settings->raw.features.motion_photo.enabled &&
                              state.replay_buffer &&
                              state.replay_buffer->status.load(std::memory_order_acquire) ==
                                  Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering;

  // 创建截图完成回调
  auto completion_callback = [&state, motion_photo_enabled](bool success,
                                                            const std::wstring& path) {
    if (success) {
      std::string path_str(path.begin(), path.end());

      // 如果启用了 Motion Photo，在截图完成后合成
      if (motion_photo_enabled) {
        std::filesystem::path jpeg_path(path);
        auto mp_result = Features::ReplayBuffer::UseCase::save_motion_photo(state, jpeg_path);
        if (mp_result) {
          auto mp_path_str = mp_result->string();
          Features::Notifications::show_notification(
              state, state.i18n->texts["label.app_name"],
              state.i18n->texts["message.screenshot_success"] + mp_path_str);
          Logger().debug("Motion Photo saved: {}", mp_path_str);
        } else {
          Logger().error("Motion Photo creation failed: {}", mp_result.error());
          // 回退到普通截图通知
          Features::Notifications::show_notification(
              state, state.i18n->texts["label.app_name"],
              state.i18n->texts["message.screenshot_success"] + path_str);
        }
      } else {
        Features::Notifications::show_notification(
            state, state.i18n->texts["label.app_name"],
            state.i18n->texts["message.screenshot_success"] + path_str);
        Logger().debug("Screenshot saved successfully: {}", path_str);
      }
    } else {
      Features::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                                 state.i18n->texts["message.window_adjust_failed"]);
      Logger().error("Screenshot capture failed");
    }
  };

  // 执行截图（Motion Photo 模式使用 JPEG 格式）
  auto image_format =
      motion_photo_enabled ? Utils::Image::ImageFormat::JPEG : Utils::Image::ImageFormat::PNG;
  float jpeg_quality = 1.0f;  // 视觉无损
  auto result = Features::Screenshot::take_screenshot(state, *target_window, completion_callback,
                                                      image_format, jpeg_quality);
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