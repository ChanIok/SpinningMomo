module;

module Features.Recording.UseCase;

import std;
import Core.Events;
import Core.State;
import Core.I18n.State;
import Core.Notifications;
import Core.Notifications.Types;
import Features.Recording;
import Features.Recording.Types;
import Features.Recording.State;
import Features.Settings.Types;
import Features.Settings.State;
import UI.FloatingWindow.Events;
import Features.WindowControl;
import Utils.Graphics.HDR;
import Utils.Logger;
import Utils.Media.AudioCapture;
import Utils.Path;
import Utils.String;
import Utils.System;
import <windows.h>;

namespace Features::Recording::UseCase {

auto show_recording_notification(Core::State::AppState& state, const std::string& message) -> void {
  Core::Notifications::Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = Utils::String::FromUtf8(message);
  Core::Notifications::post_notification_request(state, std::move(options));
}

auto show_recording_saved_notification(Core::State::AppState& state,
                                       const std::filesystem::path& saved_path) -> void {
  Core::Notifications::Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message =
      Utils::String::FromUtf8(state.i18n->texts["message.recording_saved"]) + saved_path.wstring();

  Core::Notifications::Types::NotificationAction view_action;
  view_action.label = Utils::String::FromUtf8(state.i18n->texts["notification.action.view"]);
  view_action.callback = [saved_path](Core::State::AppState&) {
    auto open_result = Utils::System::open_file_with_default_app(saved_path);
    if (!open_result) {
      Logger().warn("Failed to open recording: {}", open_result.error());
    }
  };
  options.action = std::move(view_action);

  Core::Notifications::post_notification_request(state, std::move(options));
}

auto notify_recording_toggled(Core::State::AppState& state, bool enabled) -> void {
  if (!state.events) {
    return;
  }

  Core::Events::post(state, UI::FloatingWindow::Events::RecordingToggleEvent{.enabled = enabled});
}

// 生成输出文件路径
auto generate_output_path(const Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string> {
  auto output_dir_result =
      Utils::Path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
  if (!output_dir_result) {
    return std::unexpected("Failed to get output directory: " + output_dir_result.error());
  }
  const auto& recordings_dir = output_dir_result.value();

  auto filename = Utils::String::FormatTimestamp(std::chrono::system_clock::now());
  // 与截图模块一致：FormatTimestamp 返回 .png，录制使用 .mp4
  auto dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos) + ".mp4";
  } else {
    filename += ".mp4";
  }

  return recordings_dir / filename;
}

auto toggle_recording_impl(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto status = state.recording->status.load(std::memory_order_acquire);

  if (status == Features::Recording::Types::RecordingStatus::Recording) {
    // 停止录制前先保存目标路径，stop 会清理当前录制段状态。
    std::filesystem::path saved_path = state.recording->config.output_path;
    Features::Recording::stop(state);
    std::error_code ec;
    if (std::filesystem::exists(saved_path, ec) && !ec) {
      show_recording_saved_notification(state, saved_path);
    } else {
      show_recording_notification(state, state.i18n->texts["message.recording_stop_failed"] +
                                             Utils::String::ToUtf8(saved_path.wstring()));
    }
    notify_recording_toggled(state, false);
  } else if (status == Features::Recording::Types::RecordingStatus::Idle) {
    // 开始录制

    // 1. 查找窗口
    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target = Features::WindowControl::find_target_window(window_title);
    if (!target) {
      show_recording_notification(state, state.i18n->texts["message.window_not_found"]);
      return std::unexpected("Target window not found");
    }

    // 2. 准备配置
    auto path_result = generate_output_path(state);
    if (!path_result) {
      show_recording_notification(
          state, state.i18n->texts["message.recording_start_failed"] + path_result.error());
      return std::unexpected(path_result.error());
    }

    // 从设置读取录制参数
    const auto& recording_settings = state.settings->raw.features.recording;

    Features::Recording::Types::RecordingConfig config;
    config.output_path = *path_result;
    config.fps = recording_settings.fps;
    config.bitrate = recording_settings.bitrate;
    config.quality = recording_settings.quality;
    config.qp = recording_settings.qp;
    config.rate_control =
        Features::Recording::Types::rate_control_mode_from_string(recording_settings.rate_control);
    config.encoder_mode =
        Features::Recording::Types::encoder_mode_from_string(recording_settings.encoder_mode);
    config.codec = Features::Recording::Types::video_codec_from_string(recording_settings.codec);
    config.enable_hdr = recording_settings.enable_hdr;
    config.capture_client_area = recording_settings.capture_client_area;
    config.capture_cursor = recording_settings.capture_cursor;
    config.auto_restart_on_resize = recording_settings.auto_restart_on_resize;
    config.audio_source =
        Utils::Media::AudioCapture::audio_source_from_string(recording_settings.audio_source);
    config.audio_bitrate = recording_settings.audio_bitrate;

    // 与截图一致：设置项表示「尽量 HDR」；本次是否走 HDR 管线由目标屏 hdr_active 决定。
    if (recording_settings.enable_hdr) {
      config.enable_hdr = false;
      auto hdr_info = Utils::Graphics::HDR::query_monitor_hdr_info(target.value());
      if (hdr_info) {
        config.enable_hdr = hdr_info->hdr_active;
        if (config.enable_hdr) {
          // 这个值只用于写 HDR10 静态元数据，不做 tone mapping；实际像素亮度来自 WGC scRGB 帧。
          config.hdr_target_peak_nits = static_cast<std::uint32_t>(
              std::lround(std::clamp(hdr_info->max_luminance_nits, 203.0f, 10000.0f)));
        }
      } else {
        Logger().warn("Failed to query HDR monitor info for recording: {}", hdr_info.error());
      }
    }

    if (config.enable_hdr) {
      // HDR 录制输出必须是 HEVC Main10 HDR10；在启动入口约束编码组合。
      if (config.codec != Features::Recording::Types::VideoCodec::H265) {
        std::string error = "HDR recording requires H.265 codec";
        show_recording_notification(state,
                                    state.i18n->texts["message.recording_start_failed"] + error);
        return std::unexpected(error);
      }
      if (config.encoder_mode == Features::Recording::Types::EncoderMode::CPU) {
        std::string error = "HDR recording requires GPU encoder";
        show_recording_notification(state,
                                    state.i18n->texts["message.recording_start_failed"] + error);
        return std::unexpected(error);
      }
    }

    // 3. 启动
    auto result = Features::Recording::start(state, target.value(), config);
    if (!result) {
      show_recording_notification(
          state, state.i18n->texts["message.recording_start_failed"] + result.error());
      return result;
    }

    show_recording_notification(state, state.i18n->texts["message.recording_started"]);
    notify_recording_toggled(state, true);
  } else {
    return std::unexpected("Recording is in a transitional state");
  }

  return {};
}

auto toggle_recording(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.recording) {
    return std::unexpected("Recording state is not initialized");
  }

  // shutdown 已经开始时，不再接受新的录制开关请求，避免退出阶段和 toggle 抢状态。
  if (state.recording->shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  Features::Recording::RecordingControlHandlers handlers{
      .on_toggle =
          [&state]() {
            auto result = toggle_recording_impl(state);
            if (!result) {
              Logger().error("Recording toggle failed: {}", result.error());
            }
          },
      .on_shutdown_stop =
          [&state]() {
            if (state.recording->status.load(std::memory_order_acquire) ==
                Features::Recording::Types::RecordingStatus::Recording) {
              Features::Recording::stop(state);
              notify_recording_toggled(state, false);
            }
          },
  };

  if (auto result = Features::Recording::ensure_control_thread_started(state, std::move(handlers));
      !result) {
    return result;
  }

  Features::Recording::request_control_action(
      state, Features::Recording::Types::RecordingControlAction::Toggle);

  return {};
}

auto stop_recording_if_running(Core::State::AppState& state) -> void {
  if (!state.recording) {
    return;
  }

  // 退出阶段先宣告 shutdown，再接管 stop；
  // 这样 resize restart / 用户 toggle 的控制任务会主动让路，不会再和退出抢状态。
  state.recording->shutdown_requested.store(true, std::memory_order_release);

  if (state.recording->control_thread.joinable()) {
    Features::Recording::request_control_action(
        state, Features::Recording::Types::RecordingControlAction::ShutdownStop);
    Features::Recording::join_control_thread(state);
  } else if (state.recording->status.load(std::memory_order_acquire) ==
             Features::Recording::Types::RecordingStatus::Recording) {
    // 控制线程是懒启动；没有消费者时不投递请求。此分支代表状态异常，直接兜底保存。
    Logger().warn(
        "Recording is active but control thread is not running during shutdown; stopping directly");
    Features::Recording::stop(state);
    notify_recording_toggled(state, false);
  }
}

}  // namespace Features::Recording::UseCase
