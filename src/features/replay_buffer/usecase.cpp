module;

module Features.ReplayBuffer.UseCase;

import std;
import Core.State;
import Features.ReplayBuffer;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.Trimmer;
import Features.ReplayBuffer.MotionPhoto;
import Features.Recording.State;
import Features.Recording.Types;
import Features.Settings;
import Features.Settings.State;
import Features.WindowControl;
import Utils.Logger;
import Utils.Path;
import Utils.String;

namespace Features::ReplayBuffer::UseCase {

// 内部辅助：检查是否需要后台录制
auto is_buffering_needed(Core::State::AppState& state) -> bool {
  bool motion_photo_enabled = state.settings->raw.features.motion_photo.enabled;
  bool replay_enabled =
      state.replay_buffer && state.replay_buffer->replay_enabled.load(std::memory_order_acquire);
  return motion_photo_enabled || replay_enabled;
}

// 内部辅助：构建配置
// 参数源选择：
// - 有 Instant Replay 启用 → 使用 recording 参数（高质量）
// - 仅 Motion Photo 启用 → 使用 motion_photo 参数（轻量化）
auto build_config(Core::State::AppState& state)
    -> Features::ReplayBuffer::Types::ReplayBufferConfig {
  const auto& mp_settings = state.settings->raw.features.motion_photo;
  const auto& rb_settings = state.settings->raw.features.replay_buffer;
  const auto& rec_settings = state.settings->raw.features.recording;

  bool replay_enabled =
      state.replay_buffer && state.replay_buffer->replay_enabled.load(std::memory_order_acquire);

  Features::ReplayBuffer::Types::ReplayBufferConfig config;

  if (replay_enabled) {
    // Instant Replay 启用：使用 recording 参数
    config.fps = rec_settings.fps;
    config.bitrate = rec_settings.bitrate;
    config.quality = rec_settings.quality;
    config.codec = rec_settings.codec;
    config.rate_control = rec_settings.rate_control;
    config.encoder_mode = rec_settings.encoder_mode;
    config.audio_source = rec_settings.audio_source;
    config.audio_bitrate = rec_settings.audio_bitrate;
  } else {
    // 仅 Motion Photo：使用 motion_photo 参数
    config.fps = mp_settings.fps;
    config.bitrate = mp_settings.bitrate;
    config.codec = mp_settings.codec;
    config.rate_control = "cbr";  // Motion Photo 使用 CBR
    config.encoder_mode = "auto";
    config.audio_source = mp_settings.audio_source;
    config.audio_bitrate = mp_settings.audio_bitrate;
  }

  // Motion Photo 参数
  config.motion_photo_duration = mp_settings.duration;
  config.motion_photo_resolution = mp_settings.resolution;

  // Instant Replay 参数
  config.max_duration = rb_settings.duration;

  return config;
}

auto ensure_buffering_started(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.replay_buffer) {
    return std::unexpected("ReplayBuffer state is not initialized");
  }

  auto status = state.replay_buffer->status.load(std::memory_order_acquire);

  // 已经在录制中
  if (status == Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return {};
  }

  // 检查 Recording 互斥
  if (state.recording && state.recording->status.load(std::memory_order_acquire) ==
                             Features::Recording::Types::RecordingStatus::Recording) {
    return std::unexpected("Cannot start background capture while recording");
  }

  // 查找目标窗口
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target = Features::WindowControl::find_target_window(window_title);
  if (!target) {
    return std::unexpected("Target window not found");
  }

  auto config = build_config(state);
  auto result = Features::ReplayBuffer::start_buffering(*state.replay_buffer, *target, config);
  if (!result) {
    return result;
  }

  Logger().info("Background capture started");
  return {};
}

auto ensure_buffering_stopped(Core::State::AppState& state) -> void {
  if (!state.replay_buffer) {
    return;
  }

  auto status = state.replay_buffer->status.load(std::memory_order_acquire);
  if (status == Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    Features::ReplayBuffer::stop_buffering(*state.replay_buffer);
    Logger().info("Background capture stopped");
  }
}

auto toggle_motion_photo(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.settings) {
    return std::unexpected("Settings not initialized");
  }

  // 1. 切换 enabled 状态
  bool new_enabled = !state.settings->raw.features.motion_photo.enabled;
  state.settings->raw.features.motion_photo.enabled = new_enabled;

  // 2. 保存设置到文件
  auto path_result = Features::Settings::get_settings_path();
  if (path_result) {
    auto save_result = Features::Settings::save_settings_to_file(*path_result, state.settings->raw);
    if (!save_result) {
      Logger().warn("Failed to save settings: {}", save_result.error());
    }
  }

  // 3. 根据新状态启动或停止后台录制
  if (new_enabled) {
    auto result = ensure_buffering_started(state);
    if (!result) {
      // 启动失败，回滚设置
      state.settings->raw.features.motion_photo.enabled = false;
      if (path_result) {
        (void)Features::Settings::save_settings_to_file(*path_result, state.settings->raw);
      }
      return result;
    }
    Logger().info("Motion Photo enabled");
  } else {
    // 如果即时回放也关闭，停止后台录制
    if (!is_buffering_needed(state)) {
      ensure_buffering_stopped(state);
    }
    Logger().info("Motion Photo disabled");
  }

  return {};
}

auto toggle_replay_buffer(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.replay_buffer) {
    return std::unexpected("ReplayBuffer state is not initialized");
  }

  // 1. 切换运行时状态
  bool current = state.replay_buffer->replay_enabled.load(std::memory_order_acquire);
  bool new_enabled = !current;
  state.replay_buffer->replay_enabled.store(new_enabled, std::memory_order_release);

  // 2. 根据新状态启动或停止后台录制
  if (new_enabled) {
    auto result = ensure_buffering_started(state);
    if (!result) {
      // 启动失败，回滚状态
      state.replay_buffer->replay_enabled.store(false, std::memory_order_release);
      return result;
    }
    Logger().info("Instant Replay enabled");
  } else {
    // 如果 Motion Photo 也关闭，停止后台录制
    if (!is_buffering_needed(state)) {
      ensure_buffering_stopped(state);
    }
    Logger().info("Instant Replay disabled");
  }

  return {};
}

auto save_motion_photo(Core::State::AppState& state, const std::filesystem::path& jpeg_path)
    -> std::expected<std::filesystem::path, std::string> {
  if (!state.replay_buffer) {
    return std::unexpected("ReplayBuffer state is not initialized");
  }

  if (state.replay_buffer->status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return std::unexpected("ReplayBuffer is not buffering");
  }

  // 1. 先保存原始 MP4（从环形缓冲导出）
  double duration = static_cast<double>(state.replay_buffer->config.motion_photo_duration);
  auto raw_mp4_path = state.replay_buffer->cache_dir / "motion_photo_raw.mp4";

  auto save_result =
      Features::ReplayBuffer::save_replay(*state.replay_buffer, duration, raw_mp4_path);
  if (!save_result) {
    return std::unexpected("Failed to save raw video: " + save_result.error());
  }

  // 2. 缩放/转码为 Motion Photo 格式
  auto temp_mp4_path = state.replay_buffer->cache_dir / "motion_photo_temp.mp4";

  const auto& mp_settings = state.settings->raw.features.motion_photo;
  Features::ReplayBuffer::Trimmer::ScaleConfig scale_config;
  scale_config.target_short_edge = mp_settings.resolution;
  scale_config.bitrate = mp_settings.bitrate;
  scale_config.fps = mp_settings.fps;

  // 使用 Trimmer 转码（单文件输入）
  std::vector<std::filesystem::path> input_files = {raw_mp4_path};
  auto trim_result = Features::ReplayBuffer::Trimmer::trim_and_concat(input_files, temp_mp4_path,
                                                                      duration, scale_config);

  // 清理原始文件
  std::error_code ec;
  std::filesystem::remove(raw_mp4_path, ec);

  if (!trim_result) {
    return std::unexpected("Failed to transcode video: " + trim_result.error());
  }

  // 3. 生成输出路径：将 .jpg 替换为 MP.jpg
  auto output_path =
      jpeg_path.parent_path() / (jpeg_path.stem().string() + "MP" + jpeg_path.extension().string());

  // 4. 合成 Motion Photo
  std::int64_t presentation_timestamp_us = static_cast<std::int64_t>(duration * 1'000'000);
  auto mp_result = Features::ReplayBuffer::MotionPhoto::create_motion_photo(
      jpeg_path, temp_mp4_path, output_path, presentation_timestamp_us);

  // 5. 清理临时文件
  std::filesystem::remove(temp_mp4_path, ec);
  std::filesystem::remove(jpeg_path, ec);

  if (!mp_result) {
    return std::unexpected("Failed to create Motion Photo: " + mp_result.error());
  }

  Logger().info("Motion Photo saved: {}", output_path.string());
  return output_path;
}

auto save_replay(Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string> {
  if (!state.replay_buffer) {
    return std::unexpected("ReplayBuffer state is not initialized");
  }

  if (state.replay_buffer->status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return std::unexpected("ReplayBuffer is not buffering");
  }

  // 1. 生成输出路径
  std::filesystem::path output_dir;
  const auto& output_dir_path = state.settings->raw.features.output_dir_path;

  if (!output_dir_path.empty()) {
    output_dir = std::filesystem::path(output_dir_path);
  } else {
    auto videos_dir_result = Utils::Path::GetUserVideosDirectory();
    if (videos_dir_result) {
      output_dir = *videos_dir_result / "SpinningMomo";
    } else {
      auto exe_dir_result = Utils::Path::GetExecutableDirectory();
      if (!exe_dir_result) {
        return std::unexpected("Failed to get output directory");
      }
      output_dir = *exe_dir_result / "replays";
    }
  }

  auto ensure_result = Utils::Path::EnsureDirectoryExists(output_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create output directory");
  }

  auto now = std::chrono::system_clock::now();
  auto filename = std::format("replay_{:%Y%m%d_%H%M%S}.mp4", now);
  auto output_path = output_dir / filename;

  // 2. 直接从环形缓冲导出（stream copy，无转码）
  double duration = static_cast<double>(state.replay_buffer->config.max_duration);
  auto save_result =
      Features::ReplayBuffer::save_replay(*state.replay_buffer, duration, output_path);
  if (!save_result) {
    return std::unexpected("Failed to save replay: " + save_result.error());
  }

  Logger().info("Replay saved: {}", output_path.string());
  return output_path;
}

}  // namespace Features::ReplayBuffer::UseCase
