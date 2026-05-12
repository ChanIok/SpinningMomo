module;

module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Media.AudioCapture;
import Utils.Logger;
import <audioclient.h>;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::AudioCapture {

auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }

  constexpr long double kHundredNsPerSecond = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * kHundredNsPerSecond / frequency.QuadPart);
}

auto resolve_audio_timestamp_100ns(const Features::Recording::State::RecordingState& state,
                                   UINT64 qpc_position_100ns, DWORD flags) -> std::int64_t {
  if (state.start_qpc_100ns <= 0) {
    return 0;
  }

  const bool timestamp_valid =
      qpc_position_100ns > 0 && !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR);
  const auto qpc_100ns =
      timestamp_valid ? static_cast<std::int64_t>(qpc_position_100ns) : query_qpc_100ns();

  return qpc_100ns > state.start_qpc_100ns ? qpc_100ns - state.start_qpc_100ns : 0;
}

auto initialize(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                Utils::Media::AudioCapture::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string> {
  return Utils::Media::AudioCapture::initialize(ctx, source, process_id);
}

auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void {
  Utils::Media::AudioCapture::start_capture_thread(
      state.audio,
      // is_active
      [&state]() -> bool {
        return state.accepting_input.load(std::memory_order_acquire) &&
               !state.finish_requested.load(std::memory_order_acquire) &&
               state.has_audio.load(std::memory_order_acquire);
      },
      // on_packet: 只复制音频数据并入队，SinkWriter 只在录制编码线程中使用。
      [&state](const BYTE* data, UINT32 num_frames, UINT32 bytes_per_frame,
               UINT64 qpc_position_100ns, DWORD flags) {
        if (num_frames == 0 || bytes_per_frame == 0 ||
            !state.accepting_input.load(std::memory_order_acquire)) {
          return;
        }

        State::QueuedAudioPacket packet;
        packet.num_frames = num_frames;
        packet.bytes_per_frame = bytes_per_frame;
        packet.timestamp_100ns = resolve_audio_timestamp_100ns(state, qpc_position_100ns, flags);
        if (state.audio.wave_format) {
          packet.sample_rate = state.audio.wave_format->nSamplesPerSec;
        }

        const auto byte_count = static_cast<std::size_t>(num_frames) * bytes_per_frame;
        packet.data.resize(byte_count);
        if (data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
          std::memcpy(packet.data.data(), data, byte_count);
        }

        {
          std::lock_guard queue_lock(state.queue_mutex);
          if (!state.accepting_input.load(std::memory_order_acquire)) {
            return;
          }
          if (state.audio_queue.size() >= State::k_max_audio_queue_size) {
            state.audio_queue.pop_front();
            state.dropped_audio_packets.fetch_add(1, std::memory_order_relaxed);
          }
          state.audio_queue.push_back(std::move(packet));
        }
        state.queue_cv.notify_one();
      });
}

auto stop(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void {
  Utils::Media::AudioCapture::stop(ctx);
}

auto cleanup(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void {
  Utils::Media::AudioCapture::cleanup(ctx);
}

}  // namespace Features::Recording::AudioCapture
