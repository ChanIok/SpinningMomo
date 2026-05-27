module;

#include <audioclientactivationparams.h>
#include <mmreg.h>
#include "AudioSessionTypes.h"

module Utils.Media.AudioCapture;

import std;
import Utils.Logger;
import <audioclient.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <wil/resource.h>;
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

auto create_capture_events(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                           std::string_view mode_name) -> std::expected<void, std::string> {
  ctx.audio_event.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  ctx.stop_event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!ctx.audio_event || !ctx.stop_event) {
    return std::unexpected(std::format("Failed to create audio capture events for {}", mode_name));
  }
  return {};
}

auto finish_audio_client_setup(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                               WAVEFORMATEX* format, DWORD stream_flags)
    -> std::expected<void, std::string> {
  constexpr REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms

  HRESULT hr = ctx.audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, buffer_duration,
                                            0, format, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to initialize audio client: {:08X}", static_cast<uint32_t>(hr)));
  }

  hr = ctx.audio_client->SetEventHandle(ctx.audio_event.get());
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

  return {};
}

auto create_pcm_wave_format(std::uint16_t channels, std::uint32_t sample_rate,
                            std::uint16_t bits_per_sample)
    -> std::expected<wil::unique_cotaskmem_ptr<WAVEFORMATEX>, std::string> {
  wil::unique_cotaskmem_ptr<WAVEFORMATEX> format{
      reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)))};
  if (!format) {
    return std::unexpected("Failed to allocate memory for wave format");
  }

  format->wFormatTag = WAVE_FORMAT_PCM;
  format->nChannels = channels;
  format->nSamplesPerSec = sample_rate;
  format->wBitsPerSample = bits_per_sample;
  format->nBlockAlign = format->nChannels * format->wBitsPerSample / 8;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = 0;
  return format;
}

// Process Loopback 初始化
auto initialize_process_loopback(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                                 std::uint32_t process_id) -> std::expected<void, std::string> {
  auto event_result = create_capture_events(ctx, "process loopback");
  if (!event_result) {
    return event_result;
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
  HRESULT hr =
      ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
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

  // 进程 loopback 的激活目标是虚拟设备，不通过默认播放设备协商格式。
  // 这里固定成 AAC 编码链路稳定支持的 48kHz / stereo / 16-bit PCM。
  auto wave_format_result = create_pcm_wave_format(2, 48000, 16);
  if (!wave_format_result) {
    return std::unexpected(wave_format_result.error());
  }
  ctx.wave_format = std::move(*wave_format_result);

  Logger().info("Process Loopback audio format: {} Hz, {} channels, {} bits",
                ctx.wave_format->nSamplesPerSec, ctx.wave_format->nChannels,
                ctx.wave_format->wBitsPerSample);

  auto setup_result =
      finish_audio_client_setup(ctx, ctx.wave_format.get(),
                                AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK);
  if (!setup_result) {
    return setup_result;
  }

  Logger().info("Process Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

// System Loopback 初始化
auto initialize_system_loopback(Utils::Media::AudioCapture::AudioCaptureContext& ctx)
    -> std::expected<void, std::string> {
  auto event_result = create_capture_events(ctx, "system loopback");
  if (!event_result) {
    return event_result;
  }

  wil::com_ptr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
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

  auto wave_format_result = create_pcm_wave_format(2, 48000, 16);
  if (!wave_format_result) {
    return std::unexpected(wave_format_result.error());
  }
  ctx.wave_format = std::move(*wave_format_result);

  Logger().info("System Loopback target audio format: {} Hz, {} channels, {} bits",
                ctx.wave_format->nSamplesPerSec, ctx.wave_format->nChannels,
                ctx.wave_format->wBitsPerSample);

  auto setup_result =
      finish_audio_client_setup(ctx, ctx.wave_format.get(),
                                AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK);
  if (!setup_result) {
    return setup_result;
  }

  Logger().info("System Loopback audio capture initialized: buffer size = {} frames",
                ctx.buffer_frame_count);
  return {};
}

// 通用音频捕获循环
auto audio_capture_loop(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                        std::stop_token stop_token,
                        Utils::Media::AudioCapture::AudioPacketCallback on_packet) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);
    std::stop_callback wake_on_stop(stop_token, [&ctx]() {
      if (ctx.stop_event) {
        SetEvent(ctx.stop_event.get());
      }
    });

    if (!ctx.audio_client || !ctx.capture_client || !ctx.wave_format || !ctx.audio_event ||
        !ctx.stop_event) {
      Logger().error("Audio capture loop started with incomplete context");
      return;
    }

    HRESULT hr = ctx.audio_client->Start();
    if (FAILED(hr)) {
      Logger().error("Failed to start audio client: {:08X}", static_cast<uint32_t>(hr));
      return;
    }

    Logger().info("Audio capture thread started");

    HANDLE wait_handles[] = {ctx.stop_event.get(), ctx.audio_event.get()};
    while (!stop_token.stop_requested()) {
      DWORD wait_result = WaitForMultipleObjects(static_cast<DWORD>(std::size(wait_handles)),
                                                 wait_handles, FALSE, INFINITE);
      if (wait_result == WAIT_OBJECT_0) {
        break;
      }
      if (wait_result != WAIT_OBJECT_0 + 1) {
        Logger().error("Audio capture wait failed: {}", static_cast<unsigned>(GetLastError()));
        break;
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

        if (frames_available > 0) {
          UINT32 bytes_per_frame = ctx.wave_format->nBlockAlign;
          on_packet(data, frames_available, bytes_per_frame, qpc_position, flags);
        }

        hr = ctx.capture_client->ReleaseBuffer(frames_available);
        if (FAILED(hr)) {
          Logger().error("ReleaseBuffer failed: {:08X}", static_cast<uint32_t>(hr));
          break;
        }

        hr = ctx.capture_client->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) {
          Logger().error("GetNextPacketSize failed: {:08X}", static_cast<uint32_t>(hr));
          break;
        }
      }
    }

    ctx.audio_client->Stop();
    Logger().info("Audio capture thread stopped");
  } catch (const wil::ResultException& e) {
    Logger().error("Audio capture thread COM initialization failed: {} (HRESULT: 0x{:08X})",
                   e.what(), static_cast<unsigned>(e.GetErrorCode()));
  } catch (const std::exception& e) {
    Logger().error("Audio capture thread exception: {}", e.what());
  } catch (...) {
    Logger().error("Audio capture thread exception: unknown");
  }
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
    Logger().info("Using Process Loopback mode (Game audio only)");
    return initialize_process_loopback(ctx, process_id);
  }

  Logger().info("Using System Loopback mode (All system audio)");
  return initialize_system_loopback(ctx);
}

auto start_capture_thread(AudioCaptureContext& ctx, AudioPacketCallback on_packet) -> void {
  if (!ctx.audio_client || !ctx.capture_client || !ctx.wave_format || !ctx.audio_event ||
      !ctx.stop_event) {
    Logger().error("Cannot start audio capture thread: context is not initialized");
    return;
  }

  if (ctx.capture_thread.joinable()) {
    stop(ctx);
  }

  ResetEvent(ctx.stop_event.get());
  ctx.capture_thread =
      std::jthread([&ctx, on_packet = std::move(on_packet)](std::stop_token stop_token) {
        audio_capture_loop(ctx, stop_token, on_packet);
      });
}

auto stop(AudioCaptureContext& ctx) -> void {
  if (ctx.capture_thread.joinable()) {
    ctx.capture_thread.request_stop();
  }
  if (ctx.stop_event) {
    SetEvent(ctx.stop_event.get());
  }
  if (ctx.capture_thread.joinable()) {
    ctx.capture_thread.join();
  }
}

auto cleanup(AudioCaptureContext& ctx) -> void {
  stop(ctx);

  ctx.capture_client = nullptr;
  ctx.audio_client = nullptr;
  ctx.device = nullptr;
  ctx.wave_format.reset();
  ctx.audio_event.reset();
  ctx.stop_event.reset();
  ctx.buffer_frame_count = 0;
}

}  // namespace Utils::Media::AudioCapture
