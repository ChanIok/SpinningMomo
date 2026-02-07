module;

#include <winrt/Windows.Graphics.Capture.h>

module Features.ReplayBuffer;

import std;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.DiskRingBuffer;
import Features.ReplayBuffer.Muxer;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Utils.Media.AudioCapture;
import Utils.Media.RawEncoder;
import Utils.Logger;
import Utils.Path;
import <d3d11_4.h>;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::ReplayBuffer {

// 默认缓冲文件大小限制（2GB）
constexpr std::int64_t kDefaultBufferSizeLimit = 2LL * 1024 * 1024 * 1024;

// 帧到达回调
auto on_frame_arrived(Features::ReplayBuffer::State::ReplayBufferState& state,
                      Utils::Graphics::Capture::Direct3D11CaptureFrame frame) -> void {
  if (state.status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.start_time;
  auto elapsed_100ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

  std::int64_t frame_duration_100ns = 10'000'000 / state.config.fps;
  std::int64_t target_frame_index = elapsed_100ns / frame_duration_100ns;

  auto texture =
      Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(frame.Surface());
  if (!texture) {
    Logger().error("Failed to get texture from capture frame");
    return;
  }

  std::lock_guard resource_lock(state.resource_mutex);

  while (state.frame_index <= target_frame_index) {
    std::int64_t timestamp = state.frame_index * frame_duration_100ns;

    ID3D11Texture2D* encode_texture =
        (state.frame_index < target_frame_index && state.last_encoded_texture)
            ? state.last_encoded_texture.get()
            : texture.get();

    // 使用 RawEncoder 编码
    std::expected<std::vector<Utils::Media::RawEncoder::EncodedFrame>, std::string> result;
    {
      std::lock_guard write_lock(state.encoder_write_mutex);
      result = Utils::Media::RawEncoder::encode_video_frame(state.raw_encoder, state.context.get(),
                                                            encode_texture, timestamp);
    }

    if (!result) {
      Logger().error("Failed to encode frame {}: {}", state.frame_index, result.error());
      break;
    }

    // 将编码输出写入环形缓冲
    for (auto& encoded_frame : *result) {
      auto append_result = DiskRingBuffer::append_encoded_frame(state.ring_buffer, encoded_frame);
      if (!append_result) {
        Logger().warn("Failed to append frame to ring buffer: {}", append_result.error());
      }
    }

    state.frame_index++;
  }

  // 缓存当前帧
  if (!state.last_encoded_texture) {
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;

    if (FAILED(state.device->CreateTexture2D(&desc, nullptr, state.last_encoded_texture.put()))) {
      Logger().error("Failed to create texture for frame caching");
      return;
    }
  }

  state.context->CopyResource(state.last_encoded_texture.get(), texture.get());
}

auto initialize(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string> {
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation for ReplayBuffer");
  }
  return {};
}

auto start_buffering(Features::ReplayBuffer::State::ReplayBufferState& state, HWND target_window,
                     const Features::ReplayBuffer::Types::ReplayBufferConfig& config)
    -> std::expected<void, std::string> {
  std::lock_guard resource_lock(state.resource_mutex);

  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::ReplayBuffer::Types::ReplayBufferStatus::Idle) {
    return std::unexpected("ReplayBuffer is not idle");
  }

  state.config = config;
  state.target_window = target_window;

  // 1. 创建缓存目录
  auto exe_dir = Utils::Path::GetExecutableDirectory();
  if (!exe_dir) {
    return std::unexpected("Failed to get executable directory");
  }
  state.cache_dir = *exe_dir / "cache" / "replay_buffer";

  auto ensure_result = Utils::Path::EnsureDirectoryExists(state.cache_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create cache directory: " + ensure_result.error());
  }

  // 2. 创建 Headless D3D 设备
  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    return std::unexpected("Failed to create D3D device: " + d3d_result.error());
  }
  state.device = d3d_result->first;
  state.context = d3d_result->second;

  // 启用 D3D11 多线程保护
  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(state.device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    multithread->SetMultithreadProtected(TRUE);
  }

  // 3. 创建 WinRT 设备
  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(state.device.get());
  if (!winrt_device_result) {
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
  }

  // 4. 初始化音频捕获
  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  auto audio_source = Utils::Media::AudioCapture::audio_source_from_string(config.audio_source);
  auto audio_result = Utils::Media::AudioCapture::initialize(state.audio, audio_source, process_id);
  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    Logger().info("ReplayBuffer audio capture initialized");
  }

  // 5. 获取窗口尺寸
  RECT rect;
  GetClientRect(target_window, &rect);
  int width = (rect.right - rect.left) / 2 * 2;  // 确保偶数
  int height = (rect.bottom - rect.top) / 2 * 2;

  if (width <= 0 || height <= 0) {
    return std::unexpected("Invalid window size");
  }

  // 6. 创建 RawEncoder
  Utils::Media::RawEncoder::RawEncoderConfig encoder_config;
  encoder_config.width = width;
  encoder_config.height = height;
  encoder_config.fps = config.fps;
  encoder_config.bitrate = config.bitrate;
  encoder_config.keyframe_interval = config.keyframe_interval;
  encoder_config.use_hardware = true;

  WAVEFORMATEX* wave_format = state.audio.wave_format;
  auto encoder_result =
      Utils::Media::RawEncoder::create_encoder(encoder_config, state.device.get(), wave_format);
  if (!encoder_result) {
    return std::unexpected("Failed to create RawEncoder: " + encoder_result.error());
  }
  state.raw_encoder = std::move(*encoder_result);

  // 7. 初始化硬盘环形缓冲
  auto video_type = Utils::Media::RawEncoder::get_video_output_type(state.raw_encoder);
  auto audio_type = Utils::Media::RawEncoder::get_audio_output_type(state.raw_encoder);
  auto codec_data = Utils::Media::RawEncoder::get_video_codec_private_data(state.raw_encoder);

  auto ring_result =
      DiskRingBuffer::initialize(state.ring_buffer, state.cache_dir, kDefaultBufferSizeLimit,
                                 video_type, audio_type, codec_data);
  if (!ring_result) {
    return std::unexpected("Failed to initialize ring buffer: " + ring_result.error());
  }

  // 8. 创建捕获会话（2 帧缓冲）
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, width, height,
      [&state](auto frame) { on_frame_arrived(state, frame); }, 2);

  if (!capture_result) {
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 9. 启动捕获
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  // 10. 启动音频捕获线程
  if (state.raw_encoder.has_audio) {
    Utils::Media::AudioCapture::start_capture_thread(
        state.audio,
        // get_elapsed_100ns: 相对于开始时间
        [&state]() -> std::int64_t {
          auto now = std::chrono::steady_clock::now();
          auto elapsed = now - state.start_time;
          return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;
        },
        // is_active
        [&state]() -> bool {
          return state.status.load(std::memory_order_acquire) ==
                     Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering &&
                 state.raw_encoder.has_audio;
        },
        // on_packet
        [&state](const BYTE* data, UINT32 num_frames, UINT32 bytes_per_frame,
                 std::int64_t timestamp_100ns) {
          DWORD buffer_size = num_frames * bytes_per_frame;

          std::lock_guard write_lock(state.encoder_write_mutex);
          auto result = Utils::Media::RawEncoder::encode_audio_frame(state.raw_encoder, data,
                                                                     buffer_size, timestamp_100ns);

          if (result && result->has_value()) {
            auto append_result =
                DiskRingBuffer::append_encoded_frame(state.ring_buffer, result->value());
            if (!append_result) {
              Logger().warn("Failed to append audio frame: {}", append_result.error());
            }
          }
        });
  }

  // 11. 设置状态
  state.start_time = std::chrono::steady_clock::now();
  state.frame_index = 0;
  state.status.store(Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering,
                     std::memory_order_release);

  Logger().info("ReplayBuffer started buffering (new architecture)");
  return {};
}

auto stop_buffering(Features::ReplayBuffer::State::ReplayBufferState& state) -> void {
  auto expected = Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::ReplayBuffer::Types::ReplayBufferStatus::Idle,
                                            std::memory_order_acq_rel)) {
    return;
  }

  // 1. 停止音频
  if (state.raw_encoder.has_audio) {
    Utils::Media::AudioCapture::stop(state.audio);
  }

  // 2. 停止视频捕获
  Utils::Graphics::Capture::stop_capture(state.capture_session);

  // 3. 清理资源
  {
    std::lock_guard resource_lock(state.resource_mutex);

    // Flush 并清理 RawEncoder
    Utils::Media::RawEncoder::finalize(state.raw_encoder);

    state.capture_session = {};
    state.raw_encoder = {};
    state.last_encoded_texture = nullptr;
    state.device = nullptr;
    state.context = nullptr;

    Utils::Media::AudioCapture::cleanup(state.audio);

    // 清理环形缓冲
    DiskRingBuffer::cleanup(state.ring_buffer);
  }

  Logger().info("ReplayBuffer stopped");
}

auto get_recent_frames(const Features::ReplayBuffer::State::ReplayBufferState& state,
                       double duration_seconds)
    -> std::expected<std::vector<DiskRingBuffer::FrameMetadata>, std::string> {
  return DiskRingBuffer::get_recent_frames(state.ring_buffer, duration_seconds);
}

auto save_replay(Features::ReplayBuffer::State::ReplayBufferState& state, double duration_seconds,
                 const std::filesystem::path& output_path) -> std::expected<void, std::string> {
  if (state.status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return std::unexpected("ReplayBuffer is not buffering");
  }

  // 获取最近的帧
  auto frames_result = DiskRingBuffer::get_recent_frames(state.ring_buffer, duration_seconds);
  if (!frames_result) {
    return std::unexpected("Failed to get recent frames: " + frames_result.error());
  }

  if (frames_result->empty()) {
    return std::unexpected("No frames available for replay");
  }

  // 使用 Muxer 保存为 MP4（传入文件路径以使用无锁读取）
  auto video_type = state.ring_buffer.video_media_type.get();
  auto audio_type = state.ring_buffer.audio_media_type.get();

  return Muxer::mux_frames_to_mp4(*frames_result, state.ring_buffer.data_file_path, video_type,
                                  audio_type, output_path);
}

auto cleanup(Features::ReplayBuffer::State::ReplayBufferState& state) -> void {
  stop_buffering(state);
  MFShutdown();
}

}  // namespace Features::ReplayBuffer
