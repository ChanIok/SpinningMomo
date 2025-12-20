module;

module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Logger;
import <audioclient.h>;
import <mfapi.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::AudioCapture {

// 音频捕获循环（在独立线程中运行）
auto audio_capture_loop(Features::Recording::State::RecordingState& state) -> void {
  auto& audio = state.audio;
  auto& encoder = state.encoder;

  // 启动音频流
  HRESULT hr = audio.audio_client->Start();
  if (FAILED(hr)) {
    Logger().error("Failed to start audio client: {:08X}", static_cast<uint32_t>(hr));
    return;
  }

  Logger().info("Audio capture thread started");

  while (!audio.should_stop.load()) {
    // 等待数据可用
    Sleep(10);

    // 获取可用帧数
    UINT32 packet_length = 0;
    hr = audio.capture_client->GetNextPacketSize(&packet_length);
    if (FAILED(hr)) {
      Logger().error("GetNextPacketSize failed: {:08X}", static_cast<uint32_t>(hr));
      break;
    }

    while (packet_length > 0) {
      BYTE* data = nullptr;
      UINT32 frames_available = 0;
      DWORD flags = 0;
      UINT64 device_position = 0;
      UINT64 qpc_position = 0;

      // 获取音频数据
      hr = audio.capture_client->GetBuffer(&data, &frames_available, &flags, &device_position,
                                           &qpc_position);

      if (FAILED(hr)) {
        Logger().error("GetBuffer failed: {:08X}", static_cast<uint32_t>(hr));
        break;
      }

      // 只有在非静音时才处理
      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && frames_available > 0) {
        // 计算时间戳（基于录制开始时间）
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - state.start_time;
        auto timestamp_100ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

        // 计算数据大小
        DWORD buffer_size = frames_available * audio.wave_format->nBlockAlign;

        // 创建 MF Sample
        wil::com_ptr<IMFSample> sample;
        wil::com_ptr<IMFMediaBuffer> buffer;

        if (SUCCEEDED(MFCreateSample(sample.put())) &&
            SUCCEEDED(MFCreateMemoryBuffer(buffer_size, buffer.put()))) {
          // 复制数据到 buffer
          BYTE* buffer_data = nullptr;
          if (SUCCEEDED(buffer->Lock(&buffer_data, nullptr, nullptr))) {
            std::memcpy(buffer_data, data, buffer_size);
            buffer->Unlock();
            buffer->SetCurrentLength(buffer_size);

            // 设置 sample 属性
            sample->AddBuffer(buffer.get());
            sample->SetSampleTime(timestamp_100ns);

            // 计算持续时间（100ns 单位）
            int64_t duration_100ns = static_cast<int64_t>(frames_available) * 10'000'000 /
                                     audio.wave_format->nSamplesPerSec;
            sample->SetSampleDuration(duration_100ns);

            // 写入 Sink Writer（只需加 encoder_write_mutex）
            // 检查状态使用 atomic 读取，无需锁
            if (state.status.load(std::memory_order_acquire) ==
                    Features::Recording::Types::RecordingStatus::Recording &&
                encoder.has_audio) {
              std::lock_guard write_lock(state.encoder_write_mutex);
              hr = encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get());
              if (FAILED(hr)) {
                Logger().error("Failed to write audio sample: {:08X}", static_cast<uint32_t>(hr));
              }
            }
          }
        }
      }

      // 释放缓冲区
      hr = audio.capture_client->ReleaseBuffer(frames_available);
      if (FAILED(hr)) {
        Logger().error("ReleaseBuffer failed: {:08X}", static_cast<uint32_t>(hr));
        break;
      }

      // 获取下一个数据包大小
      hr = audio.capture_client->GetNextPacketSize(&packet_length);
      if (FAILED(hr)) {
        break;
      }
    }
  }

  // 停止音频流
  audio.audio_client->Stop();
  Logger().info("Audio capture thread stopped");
}

auto initialize(Features::Recording::State::AudioCaptureContext& ctx)
    -> std::expected<void, std::string> {
  HRESULT hr;

  // 1. 创建设备枚举器
  wil::com_ptr<IMMDeviceEnumerator> enumerator;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        IID_PPV_ARGS(enumerator.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create device enumerator: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 2. 获取默认音频输出设备（用于 Loopback）
  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, ctx.device.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get default audio endpoint: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 3. 激活音频客户端
  hr = ctx.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(ctx.audio_client.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to activate audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 4. 获取设备的混合格式
  WAVEFORMATEX* device_format = nullptr;
  hr = ctx.audio_client->GetMixFormat(&device_format);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get mix format: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Device audio format: {} Hz, {} channels, {} bits, format tag: {}",
                device_format->nSamplesPerSec, device_format->nChannels,
                device_format->wBitsPerSample, device_format->wFormatTag);

  // 5. 创建 16-bit PCM 格式（用于 AAC 编码器兼容性）
  WAVEFORMATEX pcm_format = {};
  pcm_format.wFormatTag = WAVE_FORMAT_PCM;
  pcm_format.nChannels = device_format->nChannels;
  pcm_format.nSamplesPerSec = device_format->nSamplesPerSec;
  pcm_format.wBitsPerSample = 16;  // 强制使用 16-bit
  pcm_format.nBlockAlign = pcm_format.nChannels * pcm_format.wBitsPerSample / 8;
  pcm_format.nAvgBytesPerSec = pcm_format.nSamplesPerSec * pcm_format.nBlockAlign;
  pcm_format.cbSize = 0;

  // 6. 检查设备是否支持 16-bit PCM 格式
  WAVEFORMATEX* closest_match = nullptr;
  hr = ctx.audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &pcm_format, &closest_match);

  // 选择要使用的格式
  WAVEFORMATEX* format_to_use = nullptr;
  if (hr == S_OK) {
    // 设备支持 16-bit PCM，直接使用
    Logger().info("Device supports 16-bit PCM format directly");
    ctx.wave_format = reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
    if (!ctx.wave_format) {
      CoTaskMemFree(device_format);
      if (closest_match) CoTaskMemFree(closest_match);
      return std::unexpected("Failed to allocate memory for wave format");
    }
    std::memcpy(ctx.wave_format, &pcm_format, sizeof(WAVEFORMATEX));
    format_to_use = ctx.wave_format;
  } else if (hr == S_FALSE && closest_match) {
    // 设备不支持精确格式，但提供了最接近的匹配
    Logger().info("Using closest match format: {} Hz, {} channels, {} bits",
                  closest_match->nSamplesPerSec, closest_match->nChannels,
                  closest_match->wBitsPerSample);
    ctx.wave_format = closest_match;
    format_to_use = closest_match;
    closest_match = nullptr;  // 已转移所有权
  } else {
    // 设备不支持 PCM 格式，回退到原始格式
    Logger().warn("Device does not support 16-bit PCM, using device format (may need conversion)");
    ctx.wave_format = device_format;
    format_to_use = device_format;
    device_format = nullptr;  // 已转移所有权
  }

  // 清理临时分配的内存
  if (device_format) CoTaskMemFree(device_format);
  if (closest_match) CoTaskMemFree(closest_match);

  Logger().info("Final audio format: {} Hz, {} channels, {} bits", ctx.wave_format->nSamplesPerSec,
                ctx.wave_format->nChannels, ctx.wave_format->wBitsPerSample);

  // 7. 以 Loopback 模式初始化
  REFERENCE_TIME buffer_duration = 10'000'000;  // 1 秒缓冲（100ns 单位）
  hr = ctx.audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK,  // Loopback 模式
                                    buffer_duration, 0, format_to_use, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 8. 获取捕获客户端
  hr = ctx.audio_client->GetService(__uuidof(IAudioCaptureClient),
                                    reinterpret_cast<void**>(ctx.capture_client.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get capture client: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 9. 获取缓冲区大小
  hr = ctx.audio_client->GetBufferSize(&ctx.buffer_frame_count);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get buffer size: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Audio capture initialized: buffer size = {} frames", ctx.buffer_frame_count);
  return {};
}

auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void {
  state.audio.should_stop = false;
  state.audio.capture_thread =
      std::jthread([&state](std::stop_token) { audio_capture_loop(state); });
}

auto stop(Features::Recording::State::AudioCaptureContext& ctx) -> void {
  ctx.should_stop = true;
  if (ctx.capture_thread.joinable()) {
    ctx.capture_thread.join();
  }
}

auto cleanup(Features::Recording::State::AudioCaptureContext& ctx) -> void {
  stop(ctx);

  // 释放 WASAPI 资源
  ctx.capture_client = nullptr;
  ctx.audio_client = nullptr;
  ctx.device = nullptr;

  // 释放 wave format
  if (ctx.wave_format) {
    CoTaskMemFree(ctx.wave_format);
    ctx.wave_format = nullptr;
  }

  ctx.buffer_frame_count = 0;
}

}  // namespace Features::Recording::AudioCapture
