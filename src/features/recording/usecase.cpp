module;

module Features.Recording.UseCase;

import std;
import Core.Events;
import Core.State;
import Core.I18n.State;
import Core.Notifications;
import Core.Notifications.Types;
import Features.Recording;
import Features.Recording.Session;
import Features.Recording.Types;
import Features.Recording.State;
import Features.Settings.State;
import Features.WindowControl;
import UI.FloatingWindow.Events;
import Utils.Graphics.HDR;
import Utils.Logger;
import Utils.Media.AudioCapture;
import Utils.Path;
import Utils.String;
import Utils.System;
import <windows.h>;

namespace Features::Recording::UseCase {

auto show_recording_notification(Core::State::AppState& state, const std::string& message) -> void {
  // 纯文字通知，例如"录制已开始"、"窗口未找到"等简单消息
  Core::Notifications::Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = Utils::String::FromUtf8(message);
  Core::Notifications::post_notification_request(state, std::move(options));
}

auto show_recording_saved_notification(Core::State::AppState& state,
                                       const std::filesystem::path& saved_path) -> void {
  // 保存成功的通知，多一个"查看"按钮，点击就用系统默认应用打开文件
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

auto show_recording_stop_result_notification(
    Core::State::AppState& state, const Features::Recording::Types::StopResult& stop_result)
    -> void {
  // 根据 stop() 的三种结果分别显示不同通知
  using Features::Recording::Types::StopResultKind;

  switch (stop_result.kind) {
    case StopResultKind::Saved:
      // 录制成功保存，附带"查看文件"按钮
      show_recording_saved_notification(state, stop_result.output_path);
      return;

    case StopResultKind::Discarded:
      // 视频太短或编码失败，已丢弃不保存
      {
        auto detail = stop_result.error.empty()
                          ? Utils::String::ToUtf8(stop_result.output_path.wstring())
                          : stop_result.error;
        show_recording_notification(state, state.i18n->texts["message.recording_failed"] + detail);
        return;
      }

    case StopResultKind::PublishFailed:
      // 编码成功了但临时文件改名为 .mp4 时失败
      {
        auto detail = stop_result.error.empty()
                          ? Utils::String::ToUtf8(stop_result.output_path.wstring())
                          : stop_result.error;
        show_recording_notification(state,
                                    state.i18n->texts["message.recording_stop_failed"] + detail);
        return;
      }

    case StopResultKind::NotRecording:
    default:
      return;
  }
}

// 录制开关：正在录就停止，空闲就启动。启动前校验窗口、配置、HDR 条件
auto toggle_recording(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.recording) {
    return std::unexpected("Recording state is not initialized");
  }

  // shutdown 已经开始时，不再接受新的录制开关请求，避免退出阶段和 toggle 抢状态
  if (state.recording->shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  // 当前正在录制 → 按一下就是停止
  auto status = state.recording->status.load(std::memory_order_acquire);
  if (status == Features::Recording::Types::RecordingStatus::Recording) {
    auto stop_result = Features::Recording::stop(state);
    show_recording_stop_result_notification(state, stop_result);
    Core::Events::post(state, UI::FloatingWindow::Events::RecordingToggleEvent{.enabled = false});
    return {};
  }

  // 不是 Idle（例如正在 Stopping）→ 不接受新操作
  if (status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is in a transitional state");
  }

  // --- 以下是启动流程 ---

  // 先找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target = Features::WindowControl::find_target_window(window_title);
  if (!target) {
    show_recording_notification(state, state.i18n->texts["message.window_not_found"]);
    return std::unexpected("Target window not found");
  }

  // 确定输出目录
  auto output_dir_result =
      Utils::Path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
  if (!output_dir_result) {
    show_recording_notification(
        state, state.i18n->texts["message.recording_start_failed"] + output_dir_result.error());
    return std::unexpected("Failed to get output directory: " + output_dir_result.error());
  }

  // 从用户设置组装录制配置（码率、帧率、编码器、HDR 等）
  const auto& recording_settings = state.settings->raw.features.recording;
  Features::Recording::Types::RecordingConfig config;
  config.output_path =
      Features::Recording::Session::build_output_path_in_directory(*output_dir_result);
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

  // HDR 录制：先查显示器是否真的启用了 HDR，如果不是就降级为 SDR
  if (recording_settings.enable_hdr) {
    config.enable_hdr = false;
    auto hdr_info = Utils::Graphics::HDR::query_monitor_hdr_info(*target);
    if (hdr_info) {
      config.enable_hdr = hdr_info->hdr_active;
      if (config.enable_hdr) {
        config.hdr_target_peak_nits = static_cast<std::uint32_t>(
            std::lround(std::clamp(hdr_info->max_luminance_nits, 203.0f, 10000.0f)));
      }
    } else {
      Logger().warn("Failed to query HDR monitor info for recording: {}", hdr_info.error());
    }
  }

  // HDR 模式下必须用 H.265 + GPU 编码器，否则报错
  if (config.enable_hdr) {
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

  // 首次录制时懒启动控制线程；后续录制复用同一个线程
  if (auto result = Features::Recording::ensure_control_thread_started(state); !result) {
    return result;
  }

  // 提交给录制模块执行真正的 start
  auto result = Features::Recording::start(state, *target, config);
  if (!result) {
    show_recording_notification(
        state, state.i18n->texts["message.recording_start_failed"] + result.error());
    return result;
  }

  // 启动成功：发通知 + 更新 UI
  show_recording_notification(state, state.i18n->texts["message.recording_started"]);
  Core::Events::post(state, UI::FloatingWindow::Events::RecordingToggleEvent{.enabled = true});
  return {};
}

// 应用退出时停止录制：先设 shutdown 标志，再投递 ShutdownStop 到控制线程
auto stop_recording_if_running(Core::State::AppState& state) -> void {
  if (!state.recording) {
    return;
  }

  // 退出阶段先宣告 shutdown，再接管 stop。
  // 这样 resize restart / 用户 toggle 的控制任务会主动让路，不会再和退出抢状态
  state.recording->shutdown_requested.store(true, std::memory_order_release);

  if (state.recording->control_thread.joinable()) {
    // 控制线程已启动 → 投递 ShutdownStop 请求，让控制线程自己执行 stop
    Features::Recording::request_control_action(
        state, Features::Recording::Types::RecordingControlAction::ShutdownStop);
    Features::Recording::join_control_thread(state);
  } else if (state.recording->status.load(std::memory_order_acquire) ==
             Features::Recording::Types::RecordingStatus::Recording) {
    // 控制线程是懒启动的，没有消费者时它不存在。
    // 这个分支代表状态异常（没控制线程却在录制），直接兜底 stop
    Logger().warn(
        "Recording is active but control thread is not running during shutdown; stopping directly");
    (void)Features::Recording::stop(state);
  }
}

}  // namespace Features::Recording::UseCase
