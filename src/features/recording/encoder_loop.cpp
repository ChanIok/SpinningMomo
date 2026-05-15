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

// 视频帧数太少就整段丢弃、不写最终 mp4，避免误触留下半截文件。
constexpr std::uint64_t k_discard_video_frame_threshold = 3;

// ---------------------------------------------------------------------------
// 时间：全程用 QPC 转成「相对录制起点」的 100ns，和视频帧、音频时间戳一套数。
// ---------------------------------------------------------------------------

auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }
  constexpr long double kHundredNsPerSecond = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * kHundredNsPerSecond / frequency.QuadPart);
}

auto elapsed_since_start_100ns(const State::RecordingState& state) -> std::int64_t {
  if (state.start_qpc_100ns <= 0) {
    return 0;
  }
  return std::max<std::int64_t>(0, query_qpc_100ns() - state.start_qpc_100ns);
}

auto get_frame_timestamp_100ns(State::RecordingState& state,
                               Utils::Graphics::Capture::Direct3D11CaptureFrame frame)
    -> std::int64_t {
  if (state.start_qpc_100ns <= 0) {
    return 0;
  }
  return std::max<std::int64_t>(0, frame.SystemRelativeTime().count() - state.start_qpc_100ns);
}

// stop 收尾时：视频至少要写到「当前已录了多久」和「曾经出现过的最晚一包音频时间」里较大的那个，
// 否则会漏结尾声音；队列排空后单靠 elapsed 不够，所以 state 里还记了 max_seen_audio。
auto finish_target_timestamp_100ns(State::RecordingState& state) -> std::int64_t {
  const auto elapsed = elapsed_since_start_100ns(state);
  return std::max(elapsed, state.max_seen_audio_timestamp_100ns);
}

// ---------------------------------------------------------------------------
// 自有纹理：WGC 来的帧只用来拷进这张，给 MF 看的永远是我们的纹理；没新帧就反复编码它。
// ---------------------------------------------------------------------------

auto ensure_encoder_input_texture(State::RecordingState& state,
                                  const D3D11_TEXTURE2D_DESC& src_desc)
    -> std::expected<void, std::string> {
  bool need_create = !state.encoder_input_texture;
  if (!need_create) {
    D3D11_TEXTURE2D_DESC existing{};
    state.encoder_input_texture->GetDesc(&existing);
    need_create = existing.Width != src_desc.Width || existing.Height != src_desc.Height ||
                  existing.Format != src_desc.Format;
  }
  if (!need_create) {
    return {};
  }

  D3D11_TEXTURE2D_DESC d = src_desc;
  d.MipLevels = 1;
  d.ArraySize = 1;
  d.SampleDesc.Count = 1;
  d.SampleDesc.Quality = 0;
  d.Usage = D3D11_USAGE_DEFAULT;
  d.BindFlags = 0;
  d.CPUAccessFlags = 0;
  d.MiscFlags = 0;

  wil::com_ptr<ID3D11Texture2D> tex;
  if (FAILED(state.device->CreateTexture2D(&d, nullptr, tex.put()))) {
    return std::unexpected("Failed to create encoder input texture");
  }
  state.encoder_input_texture = std::move(tex);
  return {};
}

auto write_current_video_sample(State::RecordingState& state, std::int64_t timestamp_100ns)
    -> std::expected<void, std::string> {
  if (!state.encoder_input_texture || !state.has_encoder_input_texture) {
    return std::unexpected("Encoder input texture is not ready");
  }
  auto result = Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(),
                                                    state.encoder_input_texture.get(),
                                                    timestamp_100ns, state.config.fps);
  if (!result) {
    return result;
  }
  state.encoded_video_frames++;
  state.last_emitted_video_timestamp_100ns = timestamp_100ns;
  return {};
}

// 按配置 fps，从 next_video_timestamp 开始一路写到不晚于 target 为止（重复同一贴图也可以）。
auto emit_video_samples_until(State::RecordingState& state, std::int64_t target_timestamp_100ns)
    -> std::expected<void, std::string> {
  if (!state.has_encoder_input_texture || state.next_video_timestamp_100ns < 0 ||
      !state.encoder.sink_writer) {
    return {};
  }

  while (state.next_video_timestamp_100ns <= target_timestamp_100ns) {
    auto sample_result = write_current_video_sample(state, state.next_video_timestamp_100ns);
    if (!sample_result) {
      return sample_result;
    }
    state.next_video_timestamp_100ns += state.video_frame_interval_100ns;
  }
  return {};
}

// ---------------------------------------------------------------------------
// 睡眠与收尾：编码线程在 condvar 上睡，除了「有新帧 / 有音 / 要停」以外，还要按 fps 自己醒。
// ---------------------------------------------------------------------------

// 固定帧率：到了「该出下一帧视频」的时间就应当醒来补帧（哪怕 WGC 没来）。
auto encoder_needs_wake_for_fixed_fps(const State::RecordingState& state) -> bool {
  if (!state.has_encoder_input_texture || state.next_video_timestamp_100ns < 0 ||
      state.start_qpc_100ns <= 0) {
    return false;
  }
  return elapsed_since_start_100ns(state) >= state.next_video_timestamp_100ns;
}

// 音频不能超前视频：队头时间戳只要已经不晚于「最后写出来的视频时间」就可以写。
auto audio_ready_to_encode(const State::RecordingState& state) -> bool {
  if (state.audio_queue.empty() || state.last_emitted_video_timestamp_100ns < 0) {
    return false;
  }
  return state.audio_queue.front().timestamp_100ns <= state.last_emitted_video_timestamp_100ns;
}

// 离下一帧视频还有多久；没有可重复画面时就不用定时，一直睡到别的条件叫醒。
auto compute_wake_timeout(const State::RecordingState& state) -> std::chrono::nanoseconds {
  if (!state.has_encoder_input_texture || state.next_video_timestamp_100ns < 0 ||
      state.start_qpc_100ns <= 0) {
    return std::chrono::nanoseconds::max();
  }
  const auto now = elapsed_since_start_100ns(state);
  if (state.next_video_timestamp_100ns <= now) {
    return std::chrono::nanoseconds{0};
  }
  const std::int64_t diff_100ns = state.next_video_timestamp_100ns - now;
  return std::chrono::nanoseconds{diff_100ns * 100};
}

// stop 之后：没 pending、音频也写光了，还要确认视频「该补的」都补到了目标时间线后面，才退主循环。
auto should_exit_finished_segment(State::RecordingState& state, bool finishing) -> bool {
  if (!finishing) {
    return false;
  }
  std::lock_guard queue_lock(state.queue_mutex);
  if (state.video_frame_pending || !state.audio_queue.empty()) {
    return false;
  }
  if (!state.has_encoder_input_texture || state.next_video_timestamp_100ns < 0) {
    return true;
  }
  const auto target = finish_target_timestamp_100ns(state);
  return state.next_video_timestamp_100ns > target;
}

// ---------------------------------------------------------------------------
// 与 recording 启动/对外 API 衔接的辅助
// ---------------------------------------------------------------------------

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

// 音频写入：永远不超过「已经落到文件里的最后一帧视频」的时间。
auto pop_ready_audio_packet(State::RecordingState& state)
    -> std::optional<State::QueuedAudioPacket> {
  if (state.last_emitted_video_timestamp_100ns < 0) {
    return std::nullopt;
  }
  std::lock_guard queue_lock(state.queue_mutex);
  if (state.audio_queue.empty() ||
      state.audio_queue.front().timestamp_100ns > state.last_emitted_video_timestamp_100ns) {
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

auto encode_queued_audio(State::RecordingState& state, const State::QueuedAudioPacket& packet)
    -> std::expected<void, std::string> {
  auto result = write_audio_packet(state.encoder, packet);
  if (!result) {
    return result;
  }

  state.encoded_audio_packets++;
  return {};
}

// 捕获一帧：尺寸变化、裁剪、最后 Copy 到 encoder_input_texture；这里不写 MF。
auto copy_one_capture_frame_to_encoder_input(State::RecordingState& state,
                                             Utils::Graphics::Capture::Direct3D11CaptureFrame frame,
                                             const std::function<void()>& request_resize_restart)
    -> std::expected<void, std::string> {
  if (!frame) {
    return {};
  }

  auto content_size = frame.ContentSize();
  if (content_size.Width > 0 && content_size.Height > 0) {
    bool frame_size_changed = content_size.Width != state.last_frame_width ||
                              content_size.Height != state.last_frame_height;

    if (frame_size_changed) {
      auto recreate_result = Utils::Graphics::Capture::recreate_frame_pool(
          state.capture_session, content_size.Width, content_size.Height);
      if (!recreate_result) {
        return std::unexpected(recreate_result.error());
      }

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

  if (capture_plan.should_crop) {
    auto crop_result = Utils::Graphics::CaptureRegion::crop_texture_to_region(
        state.device.get(), state.context.get(), texture.get(), capture_plan.region,
        state.cropped_texture);
    if (!crop_result) {
      return std::unexpected("Failed to crop recording frame: " + crop_result.error());
    }
    current_texture = *crop_result;
  }

  D3D11_TEXTURE2D_DESC current_desc{};
  current_texture->GetDesc(&current_desc);
  if (current_desc.Width != capture_plan.output_width ||
      current_desc.Height != capture_plan.output_height) {
    return std::unexpected(std::format(
        "Recording frame size mismatch after crop: got {}x{}, expected {}x{}", current_desc.Width,
        current_desc.Height, capture_plan.output_width, capture_plan.output_height));
  }

  auto ensure_tex = ensure_encoder_input_texture(state, current_desc);
  if (!ensure_tex) {
    return ensure_tex;
  }

  state.context->CopyResource(state.encoder_input_texture.get(), current_texture);
  state.has_encoder_input_texture = true;
  return {};
}

// ---------------------------------------------------------------------------
// 模块导出的入口（声明见 encoder_loop.ixx）
// ---------------------------------------------------------------------------

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
    state.finish_requested.store(true, std::memory_order_release);
    state.video_frame_pending = true;
  }
  state.queue_cv.notify_all();
}

auto encoder_thread_proc(State::RecordingState& state, std::stop_token stop_token,
                         std::function<void()> request_resize_restart) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

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

    while (true) {
      // 睡到有活干：停录、WGC 通知、下一轮固定 fps、或者音频已经可以跟着视频写了。
      {
        std::unique_lock queue_lock(state.queue_mutex);
        while (true) {
          // 下面这些成立就该起床干活；与下面 wait/wait_for 的条件一致。
          auto should_wake_now = [&state, stop_token]() {
            const bool finishing = stop_token.stop_requested() ||
                                   state.finish_requested.load(std::memory_order_acquire);
            return stop_token.stop_requested() || finishing || state.video_frame_pending ||
                   encoder_needs_wake_for_fixed_fps(state) || audio_ready_to_encode(state);
          };

          if (should_wake_now()) {
            break;
          }

          auto ns_until_next_video = compute_wake_timeout(state);
          // 暂无「下一轮该出哪一拍视频」时钟（例如首帧没到）：只靠 WGC/音频/stop 的 notify → 无限
          // wait。 已有时钟：本来可以睡到快到下一拍，但单次睡眠上限 50ms，到时睁眼再算
          // elapsed/要不要补帧—— 停录一般用 notify_all 仍会立刻醒；上限只是防极端路径下卡住。（50ms
          // 是经验值，非硬性理论）
          constexpr auto k_wake_poll_cap_ns =
              std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(50));
          const bool no_video_clock_deadline =
              ns_until_next_video >= std::chrono::nanoseconds::max() - std::chrono::nanoseconds{1};
          if (no_video_clock_deadline) {
            state.queue_cv.wait(queue_lock, should_wake_now);
          } else {
            std::chrono::nanoseconds sleep = ns_until_next_video;
            if (sleep > k_wake_poll_cap_ns) {
              sleep = k_wake_poll_cap_ns;
            }
            state.queue_cv.wait_for(queue_lock, sleep, should_wake_now);
          }
        }
      }

      const bool finishing =
          stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire);

      // 有通知或正在收尾时，从 WGC 最多取一帧：拷进自有纹理，并推进/对齐时间线。
      if (consume_video_frame_pending(state) || finishing) {
        Utils::Graphics::Capture::Direct3D11CaptureFrame frame{nullptr};
        {
          std::lock_guard frame_lock(state.frame_mutex);
          frame = Utils::Graphics::Capture::try_get_next_frame(state.capture_session);
        }
        if (frame) {
          const auto frame_ts = get_frame_timestamp_100ns(state, frame);
          if (state.next_video_timestamp_100ns < 0) {
            // 第一段有效画面：从这一帧的采集时间开始排视频时钟。
            auto copy_result =
                copy_one_capture_frame_to_encoder_input(state, frame, request_resize_restart);
            if (!copy_result) {
              state.encoder_error = copy_result.error();
              Logger().error("Recording encoder failed: {}", copy_result.error());
              break;
            }
            if (state.has_encoder_input_texture) {
              state.next_video_timestamp_100ns = frame_ts;
            }
          } else {
            // 新画面到来前，先用旧画面把采集间隔里该给的 fps 格子填满。
            const auto emit_limit = frame_ts > 0 ? frame_ts - 1 : frame_ts;
            auto emit_before = emit_video_samples_until(state, emit_limit);
            if (!emit_before) {
              state.encoder_error = emit_before.error();
              Logger().error("Recording encoder failed: {}", emit_before.error());
              break;
            }
            auto copy_result =
                copy_one_capture_frame_to_encoder_input(state, frame, request_resize_restart);
            if (!copy_result) {
              state.encoder_error = copy_result.error();
              Logger().error("Recording encoder failed: {}", copy_result.error());
              break;
            }
          }
        }
      }

      // 把视频写到「现在该写到的时间」：正常录跟墙钟走，收尾时跟 finish_target 走。
      if (state.has_encoder_input_texture) {
        const auto target_ts =
            finishing ? finish_target_timestamp_100ns(state) : elapsed_since_start_100ns(state);
        auto emit_result = emit_video_samples_until(state, target_ts);
        if (!emit_result) {
          state.encoder_error = emit_result.error();
          Logger().error("Recording encoder failed: {}", emit_result.error());
          break;
        }
      }

      while (auto audio_packet = pop_ready_audio_packet(state)) {
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

      if (finishing && should_exit_finished_segment(state, finishing)) {
        break;
      }
    }

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
