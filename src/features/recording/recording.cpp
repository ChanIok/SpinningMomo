#include "features/recording/recording.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/audioclient.hpp"
#include "vendor/windows/mfapi.hpp"

#include "core/state/app_state.hpp"
#include "features/recording/encoder_loop.hpp"
#include "features/recording/session.hpp"
#include "features/recording/state.hpp"
#include "features/recording/time.hpp"
#include "features/recording/types.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/audio_capture.hpp"
#include "utils/media/hybrid_mp4.hpp"

namespace features::recording {

// 桌面录制的数据流与线程分工：
// WGC 帧回调只唤醒编码线程；音频采集线程复制 PCM 数据入队后唤醒编码线程。
// 编码线程按固定 fps 取帧并混流，是唯一写入 SinkWriter 的线程。
// usecase 的控制线程串行执行启动、停止和尺寸变化后的分段重启，并安排图库同步与通知。

auto start_audio_capture_thread(core::AppState& app_state) -> void {
  auto& state = *app_state.recording;

  // 启动 WASAPI 采集线程，回调会在音频数据到达时被调用
  utils::media::audio_capture::start_capture_thread(
      state.audio, [&state](const BYTE* data, UINT32 num_frames, UINT32 bytes_per_frame,
                            UINT64 qpc_position_100ns, DWORD flags) {
        // stop 后拒绝继续入队
        if (num_frames == 0 || bytes_per_frame == 0 ||
            !state.accepting_input.load(std::memory_order_acquire) ||
            state.finish_requested.load(std::memory_order_acquire) ||
            !state.has_audio.load(std::memory_order_acquire)) {
          return;
        }

        // 把 PCM 数据封装成 QueuedAudioPacket，换算时间戳到录制时间线
        QueuedAudioPacket packet;
        packet.num_frames = num_frames;
        packet.bytes_per_frame = bytes_per_frame;
        const bool timestamp_valid =
            qpc_position_100ns > 0 && !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR);
        const auto absolute_qpc_100ns = timestamp_valid
                                            ? static_cast<std::int64_t>(qpc_position_100ns)
                                            : features::recording::time::query_qpc_100ns();
        packet.timestamp_100ns = features::recording::time::relative_timestamp_100ns(
            state.start_qpc_100ns, absolute_qpc_100ns);
        if (state.audio.wave_format) {
          packet.sample_rate = state.audio.wave_format->nSamplesPerSec;
        }

        // 拷贝 PCM 数据（静音包不需要拷贝）
        const auto byte_count = static_cast<std::size_t>(num_frames) * bytes_per_frame;
        packet.data.resize(byte_count);
        if (data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
          std::memcpy(packet.data.data(), data, byte_count);
        }

        // 推入队列，队列满时丢弃最老的包
        {
          std::lock_guard queue_lock(state.queue_mutex);
          if (!state.accepting_input.load(std::memory_order_acquire)) {
            return;
          }
          if (state.audio_queue.size() >= k_max_audio_queue_size) {
            state.audio_queue.pop_front();
            state.dropped_audio_packets.fetch_add(1, std::memory_order_relaxed);
          }
          state.audio_queue.push_back(std::move(packet));
        }
        // 唤醒编码线程消费音频
        state.queue_cv.notify_one();
      });
}

// 同一请求槽按 ShutdownStop、AbortWithError、UserStop、Resize、Cleanup 的优先级合并。
auto request_control_action(core::AppState& app_state,
                            features::recording::RecordingControlAction action) -> bool {
  auto& state = *app_state.recording;

  // shutdown 后只接受 ShutdownStop，其他请求一律拒绝
  if (state.shutdown_requested.load(std::memory_order_acquire) &&
      action != RecordingControlAction::ShutdownStop) {
    return false;
  }

  {
    std::lock_guard request_lock(state.control_request_mutex);

    // UserStart 只允许覆盖空槽或 CleanupD3D，避免 UI 线程直接执行 start。
    if (action == RecordingControlAction::UserStart) {
      if (state.pending_action == RecordingControlAction::ShutdownStop ||
          state.pending_action == RecordingControlAction::AbortWithError ||
          state.pending_action == RecordingControlAction::UserStop ||
          state.pending_action == RecordingControlAction::RestartAfterResize) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // shutdown 优先级最高，直接覆盖一切 pending 请求
    if (action == RecordingControlAction::ShutdownStop) {
      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // AbortWithError 不能被 shutdown 以外的请求覆盖
    if (action == RecordingControlAction::AbortWithError) {
      if (state.pending_action == RecordingControlAction::ShutdownStop) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // 用户主动停止优先于 resize restart / cleanup，但不能覆盖 shutdown 或 abort
    if (action == RecordingControlAction::UserStop) {
      if (state.pending_action == RecordingControlAction::ShutdownStop ||
          state.pending_action == RecordingControlAction::AbortWithError) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // resize restart 可以被后来的 resize 覆盖，但不能插到 shutdown 或 abort 前面
    if (action == RecordingControlAction::RestartAfterResize) {
      if (state.pending_action == RecordingControlAction::ShutdownStop ||
          state.pending_action == RecordingControlAction::AbortWithError ||
          state.pending_action == RecordingControlAction::UserStop) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // CleanupD3D 优先级最低，前面有任一请求时都跳过
    if (action == RecordingControlAction::CleanupD3D) {
      if (state.pending_action == RecordingControlAction::ShutdownStop ||
          state.pending_action == RecordingControlAction::AbortWithError ||
          state.pending_action == RecordingControlAction::UserStop ||
          state.pending_action == RecordingControlAction::RestartAfterResize) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }
  }

  return false;
}

auto enter_stopping(core::AppState& app_state) -> bool {
  if (!app_state.recording) {
    return false;
  }

  auto& state = *app_state.recording;
  auto expected = features::recording::RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(
          expected, features::recording::RecordingStatus::Stopping, std::memory_order_acq_rel)) {
    return false;
  }

  return true;
}

auto request_restart_after_resize(core::AppState& app_state) -> void {
  // WGC 帧尺寸变化时由编码线程回调触发
  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  request_control_action(app_state, RecordingControlAction::RestartAfterResize);
}

auto initialize(core::AppState& app_state) -> std::expected<void, std::string> {
  // 初始化 Media Foundation 运行时（在应用启动时调用一次）
  (void)app_state;
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(core::AppState& app_state) -> void {
  // WGC 帧回调的简单转发：标记 pending 并唤醒编码线程
  features::recording::encoder_loop::mark_video_frame_pending(app_state);
}

auto cleanup_failed_start(core::AppState& app_state, std::string_view reason) -> void {
  auto& state = *app_state.recording;

  // start 中途失败也走一遍停输入 → 停止产帧 → 停编码线程 → 清资源的顺序，
  // 和正常 stop 的资源释放路径保持一致
  state.accepting_input.store(false, std::memory_order_release);
  features::recording::session::stop_capture_input(app_state);
  utils::media::audio_capture::stop(state.audio);
  features::recording::encoder_loop::signal_encoder_finish(app_state);

  if (state.encoder_thread.joinable()) {
    state.encoder_thread.request_stop();
    state.encoder_thread.join();
  }

  utils::media::audio_capture::cleanup(state.audio);
  features::recording::session::cleanup_capture_session(app_state);
  if (state.encoded_video_frames > k_discard_video_frame_threshold) {
    Logger().warn("Recording startup interrupted; file retained at '{}': {}",
                  state.config.output_path.string(), reason);
  } else {
    features::recording::session::delete_output_file(state.config.output_path, reason);
  }
  features::recording::session::clear_session_runtime_fields(app_state);
}

auto start(core::AppState& app_state, HWND target_window,
           const features::recording::RecordingConfig& config) -> std::expected<void, std::string> {
  if (!app_state.recording) {
    return std::unexpected("RecordingState is not initialized");
  }

  auto& state = *app_state.recording;

  // 用户主动启动会先把状态切到 Starting；自动重启仍从 Idle 开始。
  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != features::recording::RecordingStatus::Idle &&
      current_status != features::recording::RecordingStatus::Starting) {
    return std::unexpected("Recording is not idle");
  }

  if (IsIconic(target_window)) {
    return std::unexpected("Target window is minimized");
  }

  std::error_code path_error;
  if (std::filesystem::exists(config.output_path, path_error))
    return std::unexpected("Recording output already exists: " + config.output_path.string());
  if (path_error)
    return std::unexpected("Failed to inspect recording output: " + path_error.message());

  // 清空上一次录制残留的会话状态，取消 D3D 延迟释放定时器
  features::recording::session::clear_session_runtime_fields(app_state);
  features::recording::session::cancel_cleanup_timer(app_state);

  // 先算清楚这次录制的源尺寸和输出尺寸——编码器一旦创建，宽高就不能再改
  auto capture_plan_result = features::recording::session::build_startup_capture_plan(
      target_window, config.capture_client_area);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  state.config = config;
  state.target_window = target_window;
  state.capture_plan = *capture_plan_result;
  state.last_frame_width = capture_plan_result->source_width;
  state.last_frame_height = capture_plan_result->source_height;
  state.config.width = static_cast<std::uint32_t>(capture_plan_result->output_width);
  state.config.height = static_cast<std::uint32_t>(capture_plan_result->output_height);
  // 按本段 fps 算出帧间隔（100ns 单位）。默认 30fps ≈ 333333 100ns
  const auto fps = std::max<std::uint32_t>(state.config.fps, 1);
  state.video_frame_interval_100ns = std::max<std::int64_t>(1, 10'000'000LL / fps);

  // 录制间复用 D3D 设备，避免高频启停反复初始化
  auto d3d_ready_result = features::recording::session::ensure_d3d_resources_ready(app_state);
  if (!d3d_ready_result) {
    features::recording::session::clear_session_runtime_fields(app_state);
    return d3d_ready_result;
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  // 初始化音频捕获。如果 GameOnly 模式失败，降级到 System 模式
  auto audio_source_to_use = config.audio_source;
  auto audio_result =
      utils::media::audio_capture::initialize(state.audio, audio_source_to_use, process_id);
  if (!audio_result && audio_source_to_use == utils::media::audio_capture::AudioSource::GameOnly) {
    Logger().warn("GameOnly audio capture initialization failed: {}, falling back to System",
                  audio_result.error());
    utils::media::audio_capture::cleanup(state.audio);
    audio_source_to_use = utils::media::audio_capture::AudioSource::System;
    audio_result =
        utils::media::audio_capture::initialize(state.audio, audio_source_to_use, process_id);
  }

  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    Logger().info("Audio capture initialized");
  }

  // 先启动编码线程并等它创建好 SinkWriter，再开始 WGC 捕获。
  // 反过来的话捕获已经在产帧，编码器却还没准备好
  state.encoder_thread = std::jthread([&app_state](std::stop_token stop_token) {
    features::recording::encoder_loop::encoder_thread_proc(
        app_state, stop_token, [&app_state]() { request_restart_after_resize(app_state); });
  });
  features::recording::encoder_loop::wait_encoder_ready(app_state);

  if (!state.encoder_start_succeeded) {
    // 编码器启动失败，清理已分配的资源后返回错误
    std::string error =
        state.encoder_error.empty() ? "Failed to start recording encoder" : state.encoder_error;
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected(error);
  }

  utils::graphics::capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;
  if (fps > 60) {
    // 高帧率录制显式请求更短的 WGC 更新间隔，绕开部分系统版本默认 60Hz 节流。
    capture_options.min_update_interval = std::chrono::milliseconds(1);
  }
  if (config.enable_hdr) {
    // HDR 桌面捕获使用 scRGB 16-bit float，后续编码线程会转成 HEVC 需要的 P010
    capture_options.pixel_format =
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // WGC 的回调只打标记不拷数据，真正的取帧和编码都在编码线程里完成
  auto capture_result = utils::graphics::capture::create_capture_session_with_frame_notification(
      target_window, state.winrt_device, state.capture_plan.source_width,
      state.capture_plan.source_height, [&app_state]() { on_frame_arrived(app_state); }, 3,
      capture_options);

  if (!capture_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 冻结录制起点时间戳，之后所有视频帧和音频都基于这个时间计算偏移
  state.start_qpc_100ns = features::recording::time::query_qpc_100ns();
  state.accepting_input.store(true, std::memory_order_release);

  // 正式启动 WGC 捕获。从此 on_frame_arrived 可能随时被调用
  auto start_result = utils::graphics::capture::start_capture(state.capture_session);
  if (!start_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  state.status.store(features::recording::RecordingStatus::Recording, std::memory_order_release);

  // 如果音频设备初始化成功，启动音频采集线程
  if (state.has_audio.load(std::memory_order_acquire)) {
    start_audio_capture_thread(app_state);
  }

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

auto stop(core::AppState& app_state) -> features::recording::StopResult {
  features::recording::StopResult result;

  if (!app_state.recording) {
    return result;
  }

  auto& state = *app_state.recording;
  result.output_path = state.config.output_path;

  if (state.status.load(std::memory_order_acquire) == RecordingStatus::Recording) {
    (void)enter_stopping(app_state);
  }
  if (state.status.load(std::memory_order_acquire) != RecordingStatus::Stopping) {
    return result;
  }

  // 第一步：关入口，已经在队列里的数据继续处理，新来的拒绝
  state.accepting_input.store(false, std::memory_order_release);

  // 第二步：停 WGC 产帧，但保留 frame pool 让编码线程排空已到达的帧
  auto stop_capture_start = std::chrono::steady_clock::now();
  features::recording::session::stop_capture_input(app_state);
  Logger().debug("Recording capture stopped in {}ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - stop_capture_start)
                     .count());

  // 第三步：停音频采集
  if (state.has_audio.load(std::memory_order_acquire)) {
    auto stop_audio_start = std::chrono::steady_clock::now();
    utils::media::audio_capture::stop(state.audio);
    Logger().debug("Recording audio stopped in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - stop_audio_start)
                       .count());
  }

  // 第四步：冻结 stop 的目标时间线。之后编码线程只能追这个固定目标，不能继续跟着墙钟走
  state.frozen_finish_target_100ns.store(
      features::recording::time::elapsed_since_start_100ns(state.start_qpc_100ns),
      std::memory_order_release);

  // 第五步：通知编码线程收尾，等它把队列排空并 finalize 编码器
  features::recording::encoder_loop::signal_encoder_finish(app_state);

  if (state.encoder_thread.joinable()) {
    Logger().debug("Waiting for recording encoder thread to finish");
    auto join_start = std::chrono::steady_clock::now();
    state.encoder_thread.join();
    Logger().debug("Recording encoder thread joined in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - join_start)
                       .count());
  }

  // 第六步：清理音频和捕获会话
  utils::media::audio_capture::cleanup(state.audio);
  features::recording::session::cleanup_capture_session(app_state);

  // 第七步：正常结束才整理总索引，异常时直接保留原路径下的分片 MP4。
  if (state.encoded_video_frames > k_discard_video_frame_threshold) {
    result.error = state.encoder_error;
    if (result.error.empty()) {
      auto finalized = utils::media::hybrid_mp4::finalize(state.config.output_path);
      if (!finalized) result.error = finalized.error();
    }
    result.kind = result.error.empty() ? StopResultKind::Saved : StopResultKind::Failed;
    if (!result.error.empty())
      Logger().warn("Recording retained at '{}': {}", result.output_path.string(), result.error);
  } else {
    auto reason = state.encoder_error.empty() ? "too few video frames" : state.encoder_error;
    features::recording::session::delete_output_file(state.config.output_path, reason);
    result.kind = StopResultKind::Discarded;
    result.error = reason;
  }

  auto dropped_audio = state.dropped_audio_packets.load(std::memory_order_relaxed);
  if (dropped_audio > 0) {
    Logger().warn("Recording queue dropped audio packets: {}", dropped_audio);
  }
  if (state.skipped_video_frames_due_to_encoding_lag > 0) {
    Logger().warn("Recording skipped video frames due to encoding lag: {}",
                  state.skipped_video_frames_due_to_encoding_lag);
  }
  if (state.discarded_tail_audio_packets > 0) {
    Logger().info("Recording discarded tail audio packets outside video timeline: {}",
                  state.discarded_tail_audio_packets);
  }

  // 第八步：清空会话态。图库同步、通知与延迟释放策略由 usecase 安排。
  features::recording::session::clear_session_runtime_fields(app_state);
  state.status.store(features::recording::RecordingStatus::Idle, std::memory_order_release);
  Logger().info("Recording stopped");
  return result;
}

}  // namespace features::recording
