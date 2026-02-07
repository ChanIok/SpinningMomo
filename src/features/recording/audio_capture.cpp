module;

#include <audioclientactivationparams.h>
#include <wil/result.h>

module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Media.Encoder;
import Utils.Logger;
import <audioclient.h>;
import <mfapi.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <windows.h>;
import <wrl/implements.h>;

namespace {

// 版本检测：是否支持 Process Loopback API (Windows 10 2004+)
auto is_process_loopback_supported() -> bool {
  OSVERSIONINFOEXW osvi = {sizeof(osvi)};
  osvi.dwMajorVersion = 10;
  osvi.dwMinorVersion = 0;
  osvi.dwBuildNumber = 19041;  // Windows 10 2004

  DWORDLONG mask = 0;
  VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

  return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, mask);
}

// Process Loopback 激活回调类
// 必须继承 FtmBase 以支持自由线程封装，否则 ActivateAudioInterfaceAsync 会失败
class ProcessLoopbackActivator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceCompletionHandler> {
 private:
  wil::com_ptr<IAudioClient> m_audio_client;
  HRESULT m_activation_result = E_PENDING;
  wil::unique_event_nothrow m_completion_event;

 public:
  ProcessLoopbackActivator() { m_completion_event.create(); }

  // IActivateAudioInterfaceCompletionHandler
  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) {
    // 获取激活结果
    wil::com_ptr<IUnknown> audio_interface;
    HRESULT hr = operation->GetActivateResult(&m_activation_result, &audio_interface);

    if (SUCCEEDED(hr) && SUCCEEDED(m_activation_result)) {
      // 获取 IAudioClient
      audio_interface.query_to(&m_audio_client);
    }

    // 通知主线程
    m_completion_event.SetEvent();
    return S_OK;
  }

  // 等待并获取结果
  auto wait_and_get_client() -> std::expected<wil::com_ptr<IAudioClient>, std::string> {
    m_completion_event.wait();

    if (FAILED(m_activation_result)) {
      return std::unexpected(std::format("Audio activation failed: {:08X}",
                                         static_cast<uint32_t>(m_activation_result)));
    }

    if (!m_audio_client) {
      return std::unexpected("Audio client is null after activation");
    }

    return m_audio_client;
  }
};

// Process Loopback 初始化
auto initialize_process_loopback(Features::Recording::State::AudioCaptureContext& ctx,
                                 std::uint32_t process_id) -> std::expected<void, std::string> {
  HRESULT hr;

  // 0. 创建 WASAPI 缓冲就绪事件
  ctx.audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!ctx.audio_event) {
    return std::unexpected("Failed to create audio event for process loopback");
  }

  // 1. 构造激活参数
  AUDIOCLIENT_ACTIVATION_PARAMS activation_params = {};
  activation_params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  activation_params.ProcessLoopbackParams.TargetProcessId = process_id;
  activation_params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

  // 2. 包装为 PROPVARIANT
  PROPVARIANT activate_params = {};
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(activation_params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE*>(&activation_params);

  // 3. 创建回调处理器
  auto activator = Microsoft::WRL::Make<ProcessLoopbackActivator>();
  if (!activator) {
    return std::unexpected("Failed to create activation callback handler");
  }

  // 4. 异步激活
  wil::com_ptr<IActivateAudioInterfaceAsyncOperation> async_op;
  hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                   &activate_params, activator.Get(), &async_op);

  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to activate audio interface async: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 5. 等待完成并获取 AudioClient
  auto client_result = activator->wait_and_get_client();
  if (!client_result) {
    return std::unexpected(client_result.error());
  }
  ctx.audio_client = *client_result;

  // 6. 硬编码格式 (GetMixFormat 在此模式不可用)
  // 使用 48000Hz 16-bit Stereo PCM
  ctx.wave_format = reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
  if (!ctx.wave_format) {
    return std::unexpected("Failed to allocate memory for wave format");
  }

  ctx.wave_format->wFormatTag = WAVE_FORMAT_PCM;
  ctx.wave_format->nChannels = 2;
  ctx.wave_format->nSamplesPerSec = 48000;  // 48kHz
  ctx.wave_format->wBitsPerSample = 16;
  ctx.wave_format->nBlockAlign = ctx.wave_format->nChannels * ctx.wave_format->wBitsPerSample / 8;
  ctx.wave_format->nAvgBytesPerSec = ctx.wave_format->nSamplesPerSec * ctx.wave_format->nBlockAlign;
  ctx.wave_format->cbSize = 0;

  Logger().info("Process Loopback audio format: {} Hz, {} channels, {} bits",
                ctx.wave_format->nSamplesPerSec, ctx.wave_format->nChannels,
                ctx.wave_format->wBitsPerSample);

  // 7. 初始化（带自动格式转换 + 事件回调）
  REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms 缓冲
  hr = ctx.audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK |
                                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    buffer_duration, 0, ctx.wave_format, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 7.5. 绑定事件句柄
  hr = ctx.audio_client->SetEventHandle(ctx.audio_event);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set audio event handle: {:08X}", static_cast<uint32_t>(hr)));
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

  Logger().info("Process Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

}  // namespace

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
    // 等待 WASAPI 缓冲就绪事件（100ms 超时兜底，避免停止信号丢失时死等）
    if (audio.audio_event) {
      WaitForSingleObject(audio.audio_event, 100);
    } else {
      Sleep(10);  // 回退：事件未创建时保持原行为
    }

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

        // 写入编码器
        // 检查状态使用 atomic 读取，无需锁
        if (state.status.load(std::memory_order_acquire) ==
                Features::Recording::Types::RecordingStatus::Recording &&
            encoder.has_audio) {
          // 在锁外创建 sample（与原实现一致，减少持锁时间）
          DWORD buffer_size = frames_available * audio.wave_format->nBlockAlign;
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

              // 只在写入时加锁
              std::lock_guard write_lock(state.encoder_write_mutex);
              HRESULT hr =
                  encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get());
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

// System Loopback 初始化
auto initialize_system_loopback(Features::Recording::State::AudioCaptureContext& ctx)
    -> std::expected<void, std::string> {
  HRESULT hr;

  // 0. 创建 WASAPI 缓冲就绪事件
  ctx.audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!ctx.audio_event) {
    return std::unexpected("Failed to create audio event for system loopback");
  }

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

  // 7. 以 Loopback + 事件回调模式初始化
  REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms 缓冲（100ns 单位）
  hr = ctx.audio_client->Initialize(
      AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      buffer_duration, 0, format_to_use, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 7.5. 绑定事件句柄
  hr = ctx.audio_client->SetEventHandle(ctx.audio_event);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set audio event handle: {:08X}", static_cast<uint32_t>(hr)));
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

  Logger().info("System Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

// 公开 API: 初始化音频捕获（根据音频源分派）
auto initialize(Features::Recording::State::AudioCaptureContext& ctx,
                Features::Recording::Types::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string> {
  using Features::Recording::Types::AudioSource;

  // 不录制音频
  if (source == AudioSource::None) {
    Logger().info("Audio capture disabled by configuration");
    return {};
  }

  // 如果选择仅游戏音频，先检查系统支持
  if (source == AudioSource::GameOnly) {
    if (!is_process_loopback_supported()) {
      Logger().warn(
          "Process Loopback API not supported (requires Windows 10 2004+), falling back to "
          "System Loopback");
      source = AudioSource::System;
    } else {
      Logger().info("Using Process Loopback mode (Game audio only)");
      return initialize_process_loopback(ctx, process_id);
    }
  }

  // 系统全部音频（默认）
  Logger().info("Using System Loopback mode (All system audio)");
  return initialize_system_loopback(ctx);
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

  // 释放音频事件
  if (ctx.audio_event) {
    CloseHandle(ctx.audio_event);
    ctx.audio_event = nullptr;
  }

  ctx.buffer_frame_count = 0;
}

}  // namespace Features::Recording::AudioCapture
