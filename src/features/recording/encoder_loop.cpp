module;

#include <wil/com.h>
#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording.EncoderLoop;

import std;
import Features.Recording.Session;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Media.Encoder;
import Utils.Media.Encoder.Types;
import Utils.Logger;
import <d3d11_4.h>;
import <mfapi.h>;
import <windows.h>;

namespace Features::Recording::EncoderLoop {

constexpr std::uint64_t k_discard_video_frame_threshold = 3;

auto build_encoder_config(const State::RecordingState& state)
    -> Utils::Media::Encoder::Types::EncoderConfig {
  Utils::Media::Encoder::Types::EncoderConfig encoder_config;
  encoder_config.output_path = state.working_output_path;
  encoder_config.width = state.config.width;
  encoder_config.height = state.config.height;
  encoder_config.fps = state.config.fps;
  encoder_config.bitrate = state.config.bitrate;
  encoder_config.quality = state.config.quality;
  encoder_config.qp = state.config.qp;
  encoder_config.keyframe_interval = 2;
  encoder_config.rate_control = Utils::Media::Encoder::Types::rate_control_mode_from_string(
      Features::Recording::Types::rate_control_mode_to_string(state.config.rate_control));
  encoder_config.encoder_mode = Utils::Media::Encoder::Types::encoder_mode_from_string(
      Features::Recording::Types::encoder_mode_to_string(state.config.encoder_mode));
  encoder_config.codec = Utils::Media::Encoder::Types::video_codec_from_string(
      Features::Recording::Types::video_codec_to_string(state.config.codec));
  encoder_config.audio_bitrate = state.config.audio_bitrate;
  return encoder_config;
}

auto notify_encoder_ready(State::RecordingState& state, bool succeeded, std::string error = {})
    -> void {
  // 编码器在编码线程里创建。start() 会等这个信号，确认能不能真正开始捕获。
  {
    std::lock_guard ready_lock(state.encoder_ready_mutex);
    state.encoder_start_succeeded = succeeded;
    state.encoder_ready = true;
    if (!error.empty()) {
      state.encoder_error = std::move(error);
    }
  }
  state.encoder_ready_cv.notify_all();
}

auto consume_video_frame_pending(State::RecordingState& state) -> bool {
  std::lock_guard queue_lock(state.queue_mutex);
  const bool pending = state.video_frame_pending;
  state.video_frame_pending = false;
  return pending;
}

auto pop_next_audio_packet(State::RecordingState& state)
    -> std::optional<State::QueuedAudioPacket> {
  std::lock_guard queue_lock(state.queue_mutex);
  if (state.audio_queue.empty()) {
    return std::nullopt;
  }

  auto packet = std::move(state.audio_queue.front());
  state.audio_queue.pop_front();
  return packet;
}

auto write_audio_packet(Utils::Media::Encoder::State::EncoderContext& encoder,
                        const State::QueuedAudioPacket& packet)
    -> std::expected<void, std::string> {
  if (!encoder.sink_writer || !encoder.has_audio || packet.data.empty()) {
    return {};
  }

  // 音频线程只给我们原始字节；写入 SinkWriter 前要包装成 Media Foundation 的 sample。
  wil::com_ptr<IMFSample> sample;
  wil::com_ptr<IMFMediaBuffer> buffer;
  const auto buffer_size = static_cast<DWORD>(packet.data.size());

  HRESULT hr = MFCreateSample(sample.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create audio sample (HRESULT: 0x{:08X})",
                                       static_cast<unsigned>(hr)));
  }

  hr = MFCreateMemoryBuffer(buffer_size, buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create audio buffer (HRESULT: 0x{:08X})",
                                       static_cast<unsigned>(hr)));
  }

  BYTE* buffer_data = nullptr;
  hr = buffer->Lock(&buffer_data, nullptr, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to lock audio buffer (HRESULT: 0x{:08X})", static_cast<unsigned>(hr)));
  }

  std::memcpy(buffer_data, packet.data.data(), packet.data.size());
  buffer->Unlock();
  buffer->SetCurrentLength(buffer_size);

  sample->AddBuffer(buffer.get());
  sample->SetSampleTime(packet.timestamp_100ns);
  if (packet.sample_rate > 0) {
    const auto duration_100ns =
        static_cast<LONGLONG>(packet.num_frames) * 10'000'000 / packet.sample_rate;
    sample->SetSampleDuration(duration_100ns);
  }

  hr = encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to write audio sample (HRESULT: 0x{:08X})", static_cast<unsigned>(hr)));
  }

  return {};
}

// WGC 的 SystemRelativeTime 使用 QPC 时间基，WASAPI 的 qpc_position 也是同一时间基。
// 这里减去录制开始时的 QPC，把视频帧时间转成“本段录制从 0 开始”的 100ns 时间戳。
auto get_frame_timestamp_100ns(State::RecordingState& state,
                               Utils::Graphics::Capture::Direct3D11CaptureFrame frame)
    -> std::int64_t {
  if (state.start_qpc_100ns <= 0) {
    return 0;
  }

  return std::max<std::int64_t>(0, frame.SystemRelativeTime().count() - state.start_qpc_100ns);
}

auto ensure_video_timeline_until(State::RecordingState& state, std::int64_t target_timestamp_100ns)
    -> std::expected<void, std::string> {
  if (!state.encoder.sink_writer || target_timestamp_100ns < 0) {
    return {};
  }

  // 这里不生成重复画面，只向 MF 声明 video stream 在这些帧点没有 sample。
  // 这样音频继续写入时，SinkWriter 不会看到“音频时间线前进，视频流毫无交代地停住”。
  // 保留半帧容忍，避免把正常调度抖动误判成缺帧，并避免 tick 紧贴真实帧。
  while (state.video_timeline.next_expected_timestamp_100ns +
             state.video_timeline.half_frame_100ns <=
         target_timestamp_100ns) {
    const auto tick_timestamp = state.video_timeline.next_expected_timestamp_100ns;
    HRESULT hr =
        state.encoder.sink_writer->SendStreamTick(state.encoder.video_stream_index, tick_timestamp);
    if (FAILED(hr)) {
      return std::unexpected(
          std::format("Failed to send video stream tick at {} (HRESULT: 0x{:08X})", tick_timestamp,
                      static_cast<unsigned>(hr)));
    }

    state.video_timeline.next_expected_timestamp_100ns += state.video_timeline.frame_interval_100ns;
    state.video_timeline.ticks_sent++;
    state.video_timeline.sample_after_gap = true;
  }

  return {};
}

auto commit_video_sample_time(State::RecordingState& state, std::int64_t timestamp_100ns) -> void {
  // 真实视频 sample 已经覆盖 timestamp_100ns，把“下一帧应出现时间”推进到后一帧。
  // 如果前面发过 tick，这个 sample 写入成功后 gap 已结束。
  const auto next_timestamp = timestamp_100ns + state.video_timeline.frame_interval_100ns;
  if (next_timestamp > state.video_timeline.next_expected_timestamp_100ns) {
    state.video_timeline.next_expected_timestamp_100ns = next_timestamp;
  }
  state.video_timeline.sample_after_gap = false;
}

auto encode_capture_frame(State::RecordingState& state,
                          Utils::Graphics::Capture::Direct3D11CaptureFrame frame,
                          const std::function<void()>& request_resize_restart)
    -> std::expected<void, std::string> {
  if (!frame) {
    return {};
  }

  // 先处理 WGC 报告的尺寸变化。输出尺寸变了就交给控制线程切段重启，
  // 输出尺寸没变则只刷新 frame pool 和裁剪计划。
  auto content_size = frame.ContentSize();
  if (content_size.Width > 0 && content_size.Height > 0) {
    bool frame_size_changed = content_size.Width != state.last_frame_width ||
                              content_size.Height != state.last_frame_height;

    if (frame_size_changed) {
      Utils::Graphics::Capture::recreate_frame_pool(state.capture_session, content_size.Width,
                                                    content_size.Height);

      auto capture_plan_result = Features::Recording::Session::resolve_capture_plan(
          state.target_window, state.config.capture_client_area, content_size.Width,
          content_size.Height);
      if (!capture_plan_result) {
        return std::unexpected("Failed to resolve recording crop plan after resize: " +
                               capture_plan_result.error());
      }

      state.last_frame_width = content_size.Width;
      state.last_frame_height = content_size.Height;

      if (state.status.load(std::memory_order_acquire) ==
              Features::Recording::Types::RecordingStatus::Recording &&
          state.config.auto_restart_on_resize &&
          (capture_plan_result->output_width != state.config.width ||
           capture_plan_result->output_height != state.config.height)) {
        if (request_resize_restart) {
          request_resize_restart();
        }
        return {};
      }

      state.capture_plan = *capture_plan_result;
    }
  }

  // WGC frame 的 Surface 是 WinRT 对象，编码前要取出底层 D3D11 texture。
  auto texture =
      Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(frame.Surface());
  if (!texture) {
    return std::unexpected("Failed to get texture from capture frame");
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  texture->GetDesc(&source_desc);
  if (source_desc.Width == 0 || source_desc.Height == 0) {
    return std::unexpected("Failed to resolve recording source texture size");
  }

  ID3D11Texture2D* current_texture = texture.get();
  auto capture_plan = state.capture_plan;
  // 正常情况下 start() 已经准备好 capture_plan；这里保留兜底刷新，
  // 防止 frame pool 重建或源尺寸变化后计划和实际纹理不一致。
  if (capture_plan.output_width == 0 || capture_plan.output_height == 0) {
    auto capture_plan_result = Features::Recording::Session::calculate_frame_crop_plan(
        state.target_window, state.config, static_cast<int>(source_desc.Width),
        static_cast<int>(source_desc.Height));
    if (!capture_plan_result) {
      return std::unexpected("Failed to resolve recording crop plan: " +
                             capture_plan_result.error());
    }
    state.capture_plan = *capture_plan_result;
    capture_plan = *capture_plan_result;
  }

  if (capture_plan.source_width != static_cast<int>(source_desc.Width) ||
      capture_plan.source_height != static_cast<int>(source_desc.Height)) {
    auto capture_plan_result = Features::Recording::Session::calculate_frame_crop_plan(
        state.target_window, state.config, static_cast<int>(source_desc.Width),
        static_cast<int>(source_desc.Height));
    if (!capture_plan_result) {
      return std::unexpected("Failed to refresh recording crop plan: " +
                             capture_plan_result.error());
    }
    state.capture_plan = *capture_plan_result;
    capture_plan = *capture_plan_result;
  }

  // 只录客户区或奇数尺寸修正时，需要先裁到编码器期望的输出尺寸。
  if (capture_plan.should_crop) {
    auto crop_result = Utils::Graphics::CaptureRegion::crop_texture_to_region(
        state.device.get(), state.context.get(), texture.get(), capture_plan.region,
        state.cropped_texture);
    if (!crop_result) {
      return std::unexpected("Failed to crop recording frame: " + crop_result.error());
    }
    current_texture = *crop_result;
  }

  // 编码器创建后宽高不能变；写入前最后确认一次实际纹理尺寸。
  D3D11_TEXTURE2D_DESC current_desc{};
  current_texture->GetDesc(&current_desc);
  if (current_desc.Width != capture_plan.output_width ||
      current_desc.Height != capture_plan.output_height) {
    return std::unexpected(std::format(
        "Recording frame size mismatch after crop: got {}x{}, expected {}x{}", current_desc.Width,
        current_desc.Height, capture_plan.output_width, capture_plan.output_height));
  }

  const auto timestamp_100ns = get_frame_timestamp_100ns(state, frame);
  if (state.encoded_video_frames > 0) {
    // 两个真实 WGC 帧之间可能隔了很久；先把中间缺失的视频帧标成 stream tick。
    auto tick_result = ensure_video_timeline_until(state, timestamp_100ns);
    if (!tick_result) {
      return tick_result;
    }
  }

  // 如果前面发过 stream tick，这一帧要标记 discontinuity，告诉编码器视频流从 gap 后恢复。
  const bool discontinuity = state.video_timeline.sample_after_gap;
  auto result =
      Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(), current_texture,
                                          timestamp_100ns, state.config.fps, discontinuity);
  if (!result) {
    return result;
  }

  state.encoded_video_frames++;
  commit_video_sample_time(state, timestamp_100ns);
  return {};
}

auto encode_available_video_frames(State::RecordingState& state,
                                   const std::function<void()>& request_resize_restart)
    -> std::expected<void, std::string> {
  std::lock_guard frame_lock(state.frame_mutex);

  while (auto frame = Utils::Graphics::Capture::try_get_next_frame(state.capture_session)) {
    auto result = encode_capture_frame(state, frame, request_resize_restart);
    if (!result) {
      return result;
    }
  }

  return {};
}

auto encode_queued_audio(State::RecordingState& state, const State::QueuedAudioPacket& packet)
    -> std::expected<void, std::string> {
  auto result = write_audio_packet(state.encoder, packet);
  if (!result) {
    return result;
  }

  state.encoded_audio_packets++;
  return {};
}

auto mark_video_frame_pending(State::RecordingState& state) -> void {
  {
    std::lock_guard queue_lock(state.queue_mutex);
    if (!state.accepting_input.load(std::memory_order_acquire)) {
      return;
    }
    state.video_frame_pending = true;
  }
  state.queue_cv.notify_one();
}

auto wait_encoder_ready(State::RecordingState& state) -> void {
  std::unique_lock ready_lock(state.encoder_ready_mutex);
  state.encoder_ready_cv.wait(ready_lock, [&state]() { return state.encoder_ready; });
}

auto signal_encoder_finish(State::RecordingState& state) -> void {
  {
    std::lock_guard queue_lock(state.queue_mutex);
    // 告诉编码线程：不会再有新数据了，把音频队列和 WGC 帧池里剩下的数据写完就收尾。
    state.finish_requested.store(true, std::memory_order_release);
    state.video_frame_pending = true;
  }
  state.queue_cv.notify_all();
}

auto encoder_thread_proc(State::RecordingState& state, std::stop_token stop_token,
                         std::function<void()> request_resize_restart) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // SinkWriter 在这个线程创建，也只在这个线程 WriteSample / Finalize。
    // 这样可以避开 Media Foundation 编码器跨线程调用时的驱动兼容性问题。
    auto encoder_config = build_encoder_config(state);
    auto encoder_result = Utils::Media::Encoder::create_encoder(encoder_config, state.device.get(),
                                                                state.audio.wave_format);
    if (!encoder_result) {
      notify_encoder_ready(state, false, "Failed to create encoder: " + encoder_result.error());
      return;
    }

    state.encoder = std::move(*encoder_result);
    state.has_audio.store(state.encoder.has_audio, std::memory_order_release);
    notify_encoder_ready(state, true);

    // 主循环：视频从 WGC frame pool 主动拉取，音频从队列取包，然后写给编码器。
    // stop 时不会立刻丢掉已到达的数据，而是先排空再 finalize。
    while (true) {
      {
        std::unique_lock queue_lock(state.queue_mutex);
        // 编码线程平时睡在这里；有新视频帧通知、新音频，或 stop 要求收尾时被叫醒。
        state.queue_cv.wait(queue_lock, [&state, stop_token]() {
          return stop_token.stop_requested() ||
                 state.finish_requested.load(std::memory_order_acquire) ||
                 state.video_frame_pending || !state.audio_queue.empty();
        });
      }

      const bool finishing =
          stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire);

      if (consume_video_frame_pending(state) || finishing) {
        auto video_result = encode_available_video_frames(state, request_resize_restart);
        if (!video_result) {
          state.encoder_error = video_result.error();
          Logger().error("Recording encoder failed: {}", video_result.error());
          break;
        }
      }

      while (auto audio_packet = pop_next_audio_packet(state)) {
        // 音频可能在 WGC 停帧期间继续推进。写音频前先补 video tick，
        // 避免 SinkWriter 内部等待一个一直没有到来的 video sample。
        auto tick_result = ensure_video_timeline_until(state, audio_packet->timestamp_100ns);
        if (!tick_result) {
          state.encoder_error = tick_result.error();
          Logger().error("Recording encoder failed: {}", tick_result.error());
          break;
        }

        auto audio_result = encode_queued_audio(state, *audio_packet);
        if (!audio_result) {
          state.encoder_error = audio_result.error();
          Logger().error("Recording encoder failed: {}", audio_result.error());
          break;
        }
      }
      if (!state.encoder_error.empty()) {
        break;
      }

      if (finishing) {
        std::lock_guard queue_lock(state.queue_mutex);
        if (!state.video_frame_pending && state.audio_queue.empty()) {
          break;
        }
      }
    }

    // 少于几个视频帧的片段大概率是误触或启动失败，直接丢弃。
    // 真正可用的片段才执行 finalize，finalize 成功后外层 stop 再改名成 .mp4。
    if (state.encoded_video_frames > k_discard_video_frame_threshold &&
        state.encoder_error.empty()) {
      auto finalize_start = std::chrono::steady_clock::now();
      auto finalize_result = Utils::Media::Encoder::finalize_encoder(state.encoder);
      auto finalize_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - finalize_start);
      if (!finalize_result) {
        state.encoder_error = finalize_result.error();
        Logger().error("Failed to finalize encoder: {}", finalize_result.error());
      } else {
        state.finalize_succeeded = true;
        Logger().info("Recording encoder finalized in {}ms", finalize_elapsed.count());
      }
    } else if (state.encoder_error.empty()) {
      Logger().info("Discard recording segment: {} encoded video frames, publish requires > {}",
                    state.encoded_video_frames, k_discard_video_frame_threshold);
    }

    state.encoder = {};
  } catch (const wil::ResultException& e) {
    state.encoder_error = std::format(
        "Encoder thread COM initialization failed: {} (HRESULT: "
        "0x{:08X})",
        e.what(), static_cast<unsigned>(e.GetErrorCode()));
    Logger().error("{}", state.encoder_error);
  } catch (const std::exception& e) {
    state.encoder_error = std::format("Recording encoder thread exception: {}", e.what());
    Logger().error("{}", state.encoder_error);
  } catch (...) {
    state.encoder_error = "Recording encoder thread exception: unknown";
    Logger().error("{}", state.encoder_error);
  }
}

}  // namespace Features::Recording::EncoderLoop
