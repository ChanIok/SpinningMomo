module;

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <wil/result.h>

module Utils.Media.AudioCapture;

import std;
import Utils.Logger;
import <wil/com.h>;
import <windows.h>;
import <wrl/implements.h>;

namespace {

// Process Loopback 激活回调类
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

  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) {
    wil::com_ptr<IUnknown> audio_interface;
    HRESULT hr = operation->GetActivateResult(&m_activation_result, &audio_interface);

    if (SUCCEEDED(hr) && SUCCEEDED(m_activation_result)) {
      audio_interface.query_to(&m_audio_client);
    }

    m_completion_event.SetEvent();
    return S_OK;
  }

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
auto initialize_process_loopback(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                                 std::uint32_t process_id) -> std::expected<void, std::string> {
  HRESULT hr;

  ctx.audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!ctx.audio_event) {
    return std::unexpected("Failed to create audio event for process loopback");
  }

  AUDIOCLIENT_ACTIVATION_PARAMS activation_params = {};
  activation_params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  activation_params.ProcessLoopbackParams.TargetProcessId = process_id;
  activation_params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

  PROPVARIANT activate_params = {};
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(activation_params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE*>(&activation_params);

  auto activator = Microsoft::WRL::Make<ProcessLoopbackActivator>();
  if (!activator) {
    return std::unexpected("Failed to create activation callback handler");
  }

  wil::com_ptr<IActivateAudioInterfaceAsyncOperation> async_op;
  hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                   &activate_params, activator.Get(), &async_op);

  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to activate audio interface async: {:08X}", static_cast<uint32_t>(hr)));
  }

  auto client_result = activator->wait_and_get_client();
  if (!client_result) {
    return std::unexpected(client_result.error());
  }
  ctx.audio_client = *client_result;

  // 硬编码格式 (GetMixFormat 在此模式不可用)
  ctx.wave_format = reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
  if (!ctx.wave_format) {
    return std::unexpected("Failed to allocate memory for wave format");
  }

  ctx.wave_format->wFormatTag = WAVE_FORMAT_PCM;
  ctx.wave_format->nChannels = 2;
  ctx.wave_format->nSamplesPerSec = 48000;
  ctx.wave_format->wBitsPerSample = 16;
  ctx.wave_format->nBlockAlign = ctx.wave_format->nChannels * ctx.wave_format->wBitsPerSample / 8;
  ctx.wave_format->nAvgBytesPerSec = ctx.wave_format->nSamplesPerSec * ctx.wave_format->nBlockAlign;
  ctx.wave_format->cbSize = 0;

  Logger().info("Process Loopback audio format: {} Hz, {} channels, {} bits",
                ctx.wave_format->nSamplesPerSec, ctx.wave_format->nChannels,
                ctx.wave_format->wBitsPerSample);

  REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms
  hr = ctx.audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK |
                                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    buffer_duration, 0, ctx.wave_format, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->SetEventHandle(ctx.audio_event);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set audio event handle: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->GetService(__uuidof(IAudioCaptureClient),
                                    reinterpret_cast<void**>(ctx.capture_client.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get capture client: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->GetBufferSize(&ctx.buffer_frame_count);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get buffer size: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Process Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

// System Loopback 初始化
auto initialize_system_loopback(Utils::Media::AudioCapture::AudioCaptureContext& ctx)
    -> std::expected<void, std::string> {
  HRESULT hr;

  ctx.audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!ctx.audio_event) {
    return std::unexpected("Failed to create audio event for system loopback");
  }

  wil::com_ptr<IMMDeviceEnumerator> enumerator;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        IID_PPV_ARGS(enumerator.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create device enumerator: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, ctx.device.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get default audio endpoint: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(ctx.audio_client.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to activate audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  WAVEFORMATEX* device_format = nullptr;
  hr = ctx.audio_client->GetMixFormat(&device_format);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get mix format: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Device audio format: {} Hz, {} channels, {} bits, format tag: {}",
                device_format->nSamplesPerSec, device_format->nChannels,
                device_format->wBitsPerSample, device_format->wFormatTag);

  // 创建 16-bit PCM 格式（用于 AAC 编码器兼容性）
  WAVEFORMATEX pcm_format = {};
  pcm_format.wFormatTag = WAVE_FORMAT_PCM;
  pcm_format.nChannels = device_format->nChannels;
  pcm_format.nSamplesPerSec = device_format->nSamplesPerSec;
  pcm_format.wBitsPerSample = 16;
  pcm_format.nBlockAlign = pcm_format.nChannels * pcm_format.wBitsPerSample / 8;
  pcm_format.nAvgBytesPerSec = pcm_format.nSamplesPerSec * pcm_format.nBlockAlign;
  pcm_format.cbSize = 0;

  WAVEFORMATEX* closest_match = nullptr;
  hr = ctx.audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &pcm_format, &closest_match);

  WAVEFORMATEX* format_to_use = nullptr;
  if (hr == S_OK) {
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
    Logger().info("Using closest match format: {} Hz, {} channels, {} bits",
                  closest_match->nSamplesPerSec, closest_match->nChannels,
                  closest_match->wBitsPerSample);
    ctx.wave_format = closest_match;
    format_to_use = closest_match;
    closest_match = nullptr;
  } else {
    Logger().warn("Device does not support 16-bit PCM, using device format (may need conversion)");
    ctx.wave_format = device_format;
    format_to_use = device_format;
    device_format = nullptr;
  }

  if (device_format) CoTaskMemFree(device_format);
  if (closest_match) CoTaskMemFree(closest_match);

  Logger().info("Final audio format: {} Hz, {} channels, {} bits", ctx.wave_format->nSamplesPerSec,
                ctx.wave_format->nChannels, ctx.wave_format->wBitsPerSample);

  REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms
  hr = ctx.audio_client->Initialize(
      AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      buffer_duration, 0, format_to_use, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->SetEventHandle(ctx.audio_event);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set audio event handle: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->GetService(__uuidof(IAudioCaptureClient),
                                    reinterpret_cast<void**>(ctx.capture_client.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get capture client: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->GetBufferSize(&ctx.buffer_frame_count);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to get buffer size: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("System Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

// 通用音频捕获循环
auto audio_capture_loop(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                        std::function<std::int64_t()> get_elapsed_100ns,
                        std::function<bool()> is_active,
                        Utils::Media::AudioCapture::AudioPacketCallback on_packet) -> void {
  HRESULT hr = ctx.audio_client->Start();
  if (FAILED(hr)) {
    Logger().error("Failed to start audio client: {:08X}", static_cast<uint32_t>(hr));
    return;
  }

  Logger().info("Audio capture thread started");

  while (!ctx.should_stop.load()) {
    if (ctx.audio_event) {
      WaitForSingleObject(ctx.audio_event, 100);
    } else {
      Sleep(10);
    }

    UINT32 packet_length = 0;
    hr = ctx.capture_client->GetNextPacketSize(&packet_length);
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

      hr = ctx.capture_client->GetBuffer(&data, &frames_available, &flags, &device_position,
                                         &qpc_position);

      if (FAILED(hr)) {
        Logger().error("GetBuffer failed: {:08X}", static_cast<uint32_t>(hr));
        break;
      }

      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && frames_available > 0) {
        if (is_active()) {
          auto timestamp_100ns = get_elapsed_100ns();
          UINT32 bytes_per_frame = ctx.wave_format->nBlockAlign;
          on_packet(data, frames_available, bytes_per_frame, timestamp_100ns);
        }
      }

      hr = ctx.capture_client->ReleaseBuffer(frames_available);
      if (FAILED(hr)) {
        Logger().error("ReleaseBuffer failed: {:08X}", static_cast<uint32_t>(hr));
        break;
      }

      hr = ctx.capture_client->GetNextPacketSize(&packet_length);
      if (FAILED(hr)) {
        break;
      }
    }
  }

  ctx.audio_client->Stop();
  Logger().info("Audio capture thread stopped");
}

}  // namespace

namespace Utils::Media::AudioCapture {

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

auto initialize(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string> {
  if (source == AudioSource::None) {
    Logger().info("Audio capture disabled by configuration");
    return {};
  }

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

  Logger().info("Using System Loopback mode (All system audio)");
  return initialize_system_loopback(ctx);
}

auto start_capture_thread(AudioCaptureContext& ctx, std::function<std::int64_t()> get_elapsed_100ns,
                          std::function<bool()> is_active, AudioPacketCallback on_packet) -> void {
  ctx.should_stop = false;
  ctx.capture_thread = std::jthread([&ctx, get_elapsed_100ns = std::move(get_elapsed_100ns),
                                     is_active = std::move(is_active),
                                     on_packet = std::move(on_packet)](std::stop_token) {
    audio_capture_loop(ctx, get_elapsed_100ns, is_active, on_packet);
  });
}

auto stop(AudioCaptureContext& ctx) -> void {
  ctx.should_stop = true;
  if (ctx.capture_thread.joinable()) {
    ctx.capture_thread.join();
  }
}

auto cleanup(AudioCaptureContext& ctx) -> void {
  stop(ctx);

  ctx.capture_client = nullptr;
  ctx.audio_client = nullptr;
  ctx.device = nullptr;

  if (ctx.wave_format) {
    CoTaskMemFree(ctx.wave_format);
    ctx.wave_format = nullptr;
  }

  if (ctx.audio_event) {
    CloseHandle(ctx.audio_event);
    ctx.audio_event = nullptr;
  }

  ctx.buffer_frame_count = 0;
}

}  // namespace Utils::Media::AudioCapture
