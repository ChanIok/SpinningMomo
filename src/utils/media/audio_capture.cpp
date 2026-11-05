#include "utils/media/audio_capture.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/audioclient.hpp"
#include "vendor/windows/audioclientactivationparams.hpp"
#include "vendor/windows/mmdeviceapi.hpp"
#include "vendor/windows/mmreg.hpp"
#include "vendor/windows/wrl/implements.hpp"

#include "utils/logger/logger.hpp"
#include "vendor/windows/audiosessiontypes.hpp"
#include "vendor/windows/avrt.hpp"

namespace utils::media::audio_capture::detail {

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
  auto has_event() const -> bool { return bool(m_completion_event); }

  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) {
    wil::com_ptr<IUnknown> audio_interface;
    HRESULT hr = operation->GetActivateResult(&m_activation_result, &audio_interface);

    if (FAILED(hr)) m_activation_result = hr;
    if (SUCCEEDED(hr) && SUCCEEDED(m_activation_result)) {
      audio_interface.query_to(&m_audio_client);
    }

    m_completion_event.SetEvent();
    return S_OK;
  }

  auto wait_and_get_client(HANDLE stop_event)
      -> std::expected<wil::com_ptr<IAudioClient>, std::string> {
    if (!m_completion_event) return std::unexpected("Cannot create audio activation event");
    HANDLE events[] = {stop_event, m_completion_event.get()};
    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
      return std::unexpected("Audio activation cancelled");

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

auto finish_audio_client_setup(utils::media::audio_capture::AudioCaptureContext& ctx,
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
auto initialize_process_loopback(utils::media::audio_capture::AudioCaptureContext& ctx,
                                 std::uint32_t process_id) -> std::expected<void, std::string> {
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
  if (!activator || !activator->has_event()) {
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

  auto client_result = activator->wait_and_get_client(ctx.stop_event.get());
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
auto initialize_system_loopback(utils::media::audio_capture::AudioCaptureContext& ctx)
    -> std::expected<void, std::string> {
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

// 通知只负责唤醒；设备初始化/释放全部由同一个采集线程执行。
class DefaultDeviceChanged final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IMMNotificationClient> {
 public:
  explicit DefaultDeviceChanged(HANDLE signal) {
    HANDLE copy = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), signal, GetCurrentProcess(), &copy, 0, FALSE,
                        DUPLICATE_SAME_ACCESS))
      signal_.reset(copy);
  }
  auto has_signal() const -> bool { return bool(signal_); }
  STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
  STDMETHODIMP OnDeviceAdded(LPCWSTR) override { return S_OK; }
  STDMETHODIMP OnDeviceRemoved(LPCWSTR) override { return S_OK; }
  STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
  STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
    if (flow == eRender && role == eConsole && signal_) SetEvent(signal_.get());
    return S_OK;
  }

 private:
  wil::unique_handle signal_;
};

auto release_client(AudioCaptureContext& ctx) -> void {
  if (ctx.audio_client) ctx.audio_client->Stop();
  ctx.capture_client.reset();
  ctx.audio_client.reset();
  ctx.device.reset();
  ctx.wave_format.reset();
}

auto read_packets(AudioCaptureContext& ctx, const AudioPacketCallback& on_packet,
                  bool cancellable = true) -> HRESULT {
  // 以WASAPI当前包数为边界；高频生产不能阻止观察停止/重连请求。
  for (unsigned count = 0; count < 64; ++count) {
    if (cancellable && WaitForSingleObject(ctx.stop_event.get(), 0) == WAIT_OBJECT_0) return S_OK;
    UINT32 size = 0;
    auto hr = ctx.capture_client->GetNextPacketSize(&size);
    if (FAILED(hr) || !size) return hr;
    BYTE* bytes = nullptr;
    UINT32 frames = 0;
    DWORD flags = 0;
    UINT64 position = 0, timestamp = 0;
    hr = ctx.capture_client->GetBuffer(&bytes, &frames, &flags, &position, &timestamp);
    if (FAILED(hr)) return hr;
    if (frames) on_packet(bytes, frames, timestamp, position, flags);
    hr = ctx.capture_client->ReleaseBuffer(frames);
    if (FAILED(hr)) return hr;
  }
  return S_OK;
}

auto capture_loop(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id,
                  AudioPacketCallback on_packet, CaptureStateCallback on_state) -> void {
  const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(com)) {
    on_state(false, "Audio capture COM initialization failed");
    return;
  }
  auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
  DWORD task = 0;
  const HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task);
  auto scheduling = wil::scope_exit([&] {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  });
  wil::com_ptr<IMMDeviceEnumerator> monitor;
  Microsoft::WRL::ComPtr<DefaultDeviceChanged> notification;
  if (source == AudioSource::System) {
    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(monitor.put()));
    if (FAILED(hr)) {
      on_state(false, "Cannot monitor default audio device");
      return;
    }
    notification = Microsoft::WRL::Make<DefaultDeviceChanged>(ctx.restart_event.get());
    if (!notification || !notification->has_signal() ||
        FAILED(monitor->RegisterEndpointNotificationCallback(notification.Get()))) {
      on_state(false, "Cannot register default audio device notification");
      return;
    }
  }
  auto unregister = wil::scope_exit([&] {
    release_client(ctx);
    if (monitor && notification)
      monitor->UnregisterEndpointNotificationCallback(notification.Get());
  });
  HANDLE retry_events[] = {ctx.stop_event.get(), ctx.restart_event.get()};
  while (WaitForSingleObject(ctx.stop_event.get(), 0) != WAIT_OBJECT_0) {
    auto initialized = source == AudioSource::GameOnly
                           ? initialize_process_loopback(ctx, process_id)
                           : initialize_system_loopback(ctx);
    HRESULT hr = initialized ? ctx.audio_client->Start() : E_FAIL;
    if (!initialized || FAILED(hr)) {
      on_state(false, initialized ? std::format("Start audio client: 0x{:08X}", unsigned(hr))
                                  : initialized.error());
      release_client(ctx);
      if (WaitForMultipleObjects(2, retry_events, FALSE, 3000) == WAIT_OBJECT_0) break;
      continue;
    }
    on_state(true, "Audio capture connected");
    HANDLE events[] = {ctx.stop_event.get(), ctx.restart_event.get(), ctx.audio_event.get()};
    bool restart = false;
    while (!restart) {
      const auto ready = WaitForMultipleObjects(3, events, FALSE, 20);
      if (ready == WAIT_OBJECT_0) {
        read_packets(ctx, on_packet, false);
        return;
      }
      if (ready == WAIT_OBJECT_0 + 1) {
        on_state(false, "Default audio device changed");
        restart = true;
      } else if (ready == WAIT_OBJECT_0 + 2 || ready == WAIT_TIMEOUT) {
        hr = read_packets(ctx, on_packet);
        if (FAILED(hr)) {
          on_state(false, std::format("Audio capture interrupted: 0x{:08X}", unsigned(hr)));
          release_client(ctx);
          if (WaitForMultipleObjects(2, retry_events, FALSE, 3000) == WAIT_OBJECT_0) return;
          restart = true;
        }
      } else {
        on_state(false, "Audio capture wait failed");
        release_client(ctx);
        if (WaitForMultipleObjects(2, retry_events, FALSE, 3000) == WAIT_OBJECT_0) return;
        restart = true;
      }
    }
    release_client(ctx);
  }
}
}  // namespace utils::media::audio_capture::detail

namespace utils::media::audio_capture {
auto is_process_loopback_supported() -> bool {
  OSVERSIONINFOEXW version{sizeof(OSVERSIONINFOEXW)};
  version.dwMajorVersion = 10;
  version.dwBuildNumber = 19041;
  DWORDLONG mask = 0;
  VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
  return VerifyVersionInfoW(&version, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, mask);
}
auto start(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id,
           AudioPacketCallback on_packet, CaptureStateCallback on_state)
    -> std::expected<void, std::string> {
  if (ctx.capture_thread.joinable()) return std::unexpected("Audio capture already started");
  if (source == AudioSource::None) return {};
  ctx.audio_event.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  ctx.restart_event.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  ctx.stop_event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!ctx.audio_event || !ctx.restart_event || !ctx.stop_event)
    return std::unexpected("Cannot create audio capture events");
  ctx.capture_thread = std::jthread(
      [&ctx, source, process_id, on_packet = std::move(on_packet), on_state = std::move(on_state)] {
        do {
          try {
            detail::capture_loop(ctx, source, process_id, on_packet, on_state);
          } catch (const std::exception& error) {
            on_state(false, error.what());
          }
          // 初始化通知或COM失败同样可重试；停止信号始终能取消等待。
        } while (WaitForSingleObject(ctx.stop_event.get(), 3000) == WAIT_TIMEOUT);
      });
  return {};
}
auto stop(AudioCaptureContext& ctx) -> void {
  if (ctx.stop_event) SetEvent(ctx.stop_event.get());
  if (ctx.capture_thread.joinable()) ctx.capture_thread.join();
}
}  // namespace utils::media::audio_capture
