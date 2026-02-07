module;

module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Media.AudioCapture;
import Utils.Logger;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::AudioCapture {

// 类型转换: Features AudioSource -> Utils AudioSource
auto convert_audio_source(Features::Recording::Types::AudioSource source)
    -> Utils::Media::AudioCapture::AudioSource {
  switch (source) {
    case Features::Recording::Types::AudioSource::None:
      return Utils::Media::AudioCapture::AudioSource::None;
    case Features::Recording::Types::AudioSource::GameOnly:
      return Utils::Media::AudioCapture::AudioSource::GameOnly;
    default:
      return Utils::Media::AudioCapture::AudioSource::System;
  }
}

auto initialize(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                Features::Recording::Types::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string> {
  return Utils::Media::AudioCapture::initialize(ctx, convert_audio_source(source), process_id);
}

auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void {
  Utils::Media::AudioCapture::start_capture_thread(
      state.audio,
      // get_elapsed_100ns
      [&state]() -> std::int64_t {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - state.start_time;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;
      },
      // is_active
      [&state]() -> bool {
        return state.status.load(std::memory_order_acquire) ==
                   Features::Recording::Types::RecordingStatus::Recording &&
               state.encoder.has_audio;
      },
      // on_packet: 在锁外创建 MF Sample，只在写入时加锁
      [&state](const BYTE* data, UINT32 num_frames, UINT32 bytes_per_frame,
               std::int64_t timestamp_100ns) {
        auto& encoder = state.encoder;
        DWORD buffer_size = num_frames * bytes_per_frame;
        wil::com_ptr<IMFSample> sample;
        wil::com_ptr<IMFMediaBuffer> buffer;

        if (SUCCEEDED(MFCreateSample(sample.put())) &&
            SUCCEEDED(MFCreateMemoryBuffer(buffer_size, buffer.put()))) {
          BYTE* buffer_data = nullptr;
          if (SUCCEEDED(buffer->Lock(&buffer_data, nullptr, nullptr))) {
            std::memcpy(buffer_data, data, buffer_size);
            buffer->Unlock();
            buffer->SetCurrentLength(buffer_size);

            sample->AddBuffer(buffer.get());
            sample->SetSampleTime(timestamp_100ns);

            std::lock_guard write_lock(state.encoder_write_mutex);
            HRESULT hr = encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get());
            if (FAILED(hr)) {
              Logger().error("Failed to write audio sample: {:08X}", static_cast<uint32_t>(hr));
            }
          }
        }
      });
}

auto stop(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void {
  Utils::Media::AudioCapture::stop(ctx);
}

auto cleanup(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void {
  Utils::Media::AudioCapture::cleanup(ctx);
}

}  // namespace Features::Recording::AudioCapture
