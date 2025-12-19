module;

module Features.Recording.UseCase;

import std;
import Core.State;
import Features.Recording;
import Features.Recording.Types;
import Features.Recording.State;
import Features.Settings.Types;
import Features.Settings.State;
import Features.WindowControl;
import UI.AppWindow.State;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import <windows.h>;

namespace Features::Recording::UseCase {

// 生成输出文件路径
auto generate_output_path(const Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string> {
  std::filesystem::path recordings_dir;

  // 从设置中读取输出目录
  const auto& output_dir_path = state.settings->raw.features.recording.output_dir_path;

  if (output_dir_path.empty()) {
    // 使用默认目录：exe/recordings/
    auto exe_dir_result = Utils::Path::GetExecutableDirectory();
    if (!exe_dir_result) {
      return std::unexpected("Failed to get executable directory");
    }
    recordings_dir = *exe_dir_result / "recordings";
  } else {
    // 使用用户指定目录
    recordings_dir = std::filesystem::path(output_dir_path);
  }

  auto ensure_result = Utils::Path::EnsureDirectoryExists(recordings_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create recordings directory");
  }

  auto now = std::chrono::system_clock::now();
  auto filename = std::format("recording_{:%Y%m%d_%H%M%S}.mp4", now);

  return recordings_dir / filename;
}

auto toggle_recording(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 如果尚未初始化，应该先初始化（如果 RecordingState 是 unique_ptr）
  if (!state.recording) {
    return std::unexpected("Recording state is not initialized");
  }

  if (state.recording->status == Features::Recording::Types::RecordingStatus::Recording) {
    // 停止录制
    Features::Recording::stop(*state.recording);

    // 更新UI状态
    if (state.app_window) {
      state.app_window->ui.recording_enabled = false;
    }
  } else if (state.recording->status == Features::Recording::Types::RecordingStatus::Idle) {
    // 开始录制

    // 1. 查找窗口
    std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
    auto target = Features::WindowControl::find_target_window(window_title);
    if (!target) {
      return std::unexpected("Target window not found");
    }

    // 2. 准备配置
    auto path_result = generate_output_path(state);
    if (!path_result) {
      return std::unexpected(path_result.error());
    }

    // 从设置读取录制参数
    const auto& recording_settings = state.settings->raw.features.recording;

    Features::Recording::Types::RecordingConfig config;
    config.output_path = *path_result;
    config.fps = recording_settings.fps;
    config.bitrate = recording_settings.bitrate;
    config.quality = recording_settings.quality;
    config.rate_control =
        Features::Recording::Types::rate_control_mode_from_string(recording_settings.rate_control);
    config.encoder_mode =
        Features::Recording::Types::encoder_mode_from_string(recording_settings.encoder_mode);
    config.codec = Features::Recording::Types::video_codec_from_string(recording_settings.codec);

    // 3. 启动
    auto result = Features::Recording::start(*state.recording, target.value(), config);
    if (!result) {
      return result;
    }

    // 更新UI状态
    if (state.app_window) {
      state.app_window->ui.recording_enabled = true;
    }
  } else {
    return std::unexpected("Recording is in a transitional state");
  }

  return {};
}

auto stop_recording_if_running(Core::State::AppState& state) -> void {
  if (state.recording &&
      state.recording->status == Features::Recording::Types::RecordingStatus::Recording) {
    Features::Recording::stop(*state.recording);
    if (state.app_window) {
      state.app_window->ui.recording_enabled = false;
    }
  }
}

}  // namespace Features::Recording::UseCase
