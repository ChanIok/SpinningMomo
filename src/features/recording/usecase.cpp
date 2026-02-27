module;

module Features.Recording.UseCase;

import std;
import Core.Events;
import Core.State;
import Core.I18n.State;
import Features.Recording;
import Features.Recording.Types;
import Features.Recording.State;
import Features.Settings.Types;
import Features.Settings.State;
import Features.WindowControl;
import UI.FloatingWindow.Events;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import <windows.h>;

namespace Features::Recording::UseCase {

auto show_recording_notification(Core::State::AppState& state, const std::string& message) -> void {
  if (!state.events || !state.i18n) {
    Logger().warn("Skip recording notification because events/i18n state is not initialized");
    return;
  }

  Core::Events::post(*state.events,
                     UI::FloatingWindow::Events::NotificationEvent{
                         .title = state.i18n->texts["label.app_name"], .message = message});
}

auto notify_recording_toggled(Core::State::AppState& state, bool enabled) -> void {
  if (!state.events) {
    return;
  }

  Core::Events::post(*state.events,
                     UI::FloatingWindow::Events::RecordingToggleEvent{.enabled = enabled});
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
    // 停止录制（保存路径在 stop 前读取，stop 不清理 config）
    std::filesystem::path saved_path = state.recording->config.output_path;
    Features::Recording::stop(*state.recording);
    show_recording_notification(state, state.i18n->texts["message.recording_saved"] +
                                           Utils::String::ToUtf8(saved_path.wstring()));
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
    config.capture_client_area = recording_settings.capture_client_area;
    config.capture_cursor = recording_settings.capture_cursor;
    config.audio_source =
        Features::Recording::Types::audio_source_from_string(recording_settings.audio_source);
    config.audio_bitrate = recording_settings.audio_bitrate;

    // 3. 启动
    auto result = Features::Recording::start(*state.recording, target.value(), config);
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

  bool expected = false;
  if (!state.recording->op_in_progress.compare_exchange_strong(expected, true,
                                                               std::memory_order_acq_rel)) {
    return {};
  }

  if (state.recording->toggle_thread.joinable()) {
    state.recording->toggle_thread.join();
  }

  state.recording->toggle_thread = std::jthread([&state](std::stop_token) {
    try {
      HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      const bool need_uninitialize = SUCCEEDED(hr);
      if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        Logger().warn("CoInitializeEx failed in recording task: {:08X}", static_cast<uint32_t>(hr));
      }

      auto result = toggle_recording_impl(state);
      if (!result) {
        Logger().error("Recording toggle failed: {}", result.error());
      }

      state.recording->op_in_progress.store(false, std::memory_order_release);

      if (need_uninitialize) {
        CoUninitialize();
      }
    } catch (const std::exception& e) {
      Logger().error("Recording toggle thread exception: {}", e.what());
      state.recording->op_in_progress.store(false, std::memory_order_release);
    } catch (...) {
      Logger().error("Recording toggle thread exception: unknown");
      state.recording->op_in_progress.store(false, std::memory_order_release);
    }
  });

  return {};
}

auto stop_recording_if_running(Core::State::AppState& state) -> void {
  if (!state.recording) {
    return;
  }

  if (state.recording->toggle_thread.joinable()) {
    state.recording->toggle_thread.join();
  }

  if (state.recording->status.load(std::memory_order_acquire) ==
      Features::Recording::Types::RecordingStatus::Recording) {
    Features::Recording::stop(*state.recording);
    notify_recording_toggled(state, false);
  }
}

}  // namespace Features::Recording::UseCase
