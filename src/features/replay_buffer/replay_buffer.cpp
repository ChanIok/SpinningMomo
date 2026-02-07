module;

#include <winrt/Windows.Graphics.Capture.h>

module Features.ReplayBuffer;

import std;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Utils.Media.AudioCapture;
import Utils.Media.Encoder;
import Utils.Media.Encoder.Types;
import Utils.Logger;
import Utils.Path;
import <d3d11_4.h>;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::ReplayBuffer {

// 生成临时段落文件路径
auto generate_segment_path(const Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::filesystem::path {
  auto now = std::chrono::system_clock::now();
  auto filename = std::format(
      "segment_{:%Y%m%d_%H%M%S}_{}.mp4", now,
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
  return state.temp_dir / filename;
}

// 创建新的编码器段落
auto create_new_segment(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string> {
  auto segment_path = generate_segment_path(state);

  // 获取窗口尺寸
  RECT rect;
  GetClientRect(state.target_window, &rect);
  int width = (rect.right - rect.left) / 2 * 2;  // 确保偶数
  int height = (rect.bottom - rect.top) / 2 * 2;

  if (width <= 0 || height <= 0) {
    return std::unexpected("Invalid window size");
  }

  // 构造编码器配置
  Utils::Media::Encoder::Types::EncoderConfig encoder_config;
  encoder_config.output_path = segment_path;
  encoder_config.width = width;
  encoder_config.height = height;
  encoder_config.fps = state.config.fps;
  encoder_config.bitrate = state.config.bitrate;
  encoder_config.quality = state.config.quality;
  encoder_config.keyframe_interval = state.config.keyframe_interval;
  encoder_config.rate_control =
      Utils::Media::Encoder::Types::rate_control_mode_from_string(state.config.rate_control);
  encoder_config.encoder_mode =
      Utils::Media::Encoder::Types::encoder_mode_from_string(state.config.encoder_mode);
  encoder_config.codec = Utils::Media::Encoder::Types::video_codec_from_string(state.config.codec);
  encoder_config.audio_bitrate = state.config.audio_bitrate;

  // 创建编码器
  WAVEFORMATEX* wave_format = state.audio.wave_format;
  auto encoder_result =
      Utils::Media::Encoder::create_encoder(encoder_config, state.device.get(), wave_format);
  if (!encoder_result) {
    return std::unexpected("Failed to create encoder: " + encoder_result.error());
  }
  state.encoder = std::move(*encoder_result);

  // 更新段落信息
  state.current_segment.path = segment_path;
  state.current_segment.start_time = std::chrono::steady_clock::now();
  state.current_segment.duration_100ns = 0;

  // 重置帧索引和段落开始时间
  state.segment_start_time = std::chrono::steady_clock::now();
  state.frame_index = 0;

  Logger().debug("New segment created: {}", segment_path.string());
  return {};
}

// 删除超出最大缓冲时长的旧段落
auto cleanup_old_segments(Features::ReplayBuffer::State::ReplayBufferState& state) -> void {
  // 计算最大保留时长（100ns 单位）
  std::int64_t max_duration_100ns =
      static_cast<std::int64_t>(state.config.max_duration) * 10'000'000;
  std::int64_t total_duration = 0;

  // 从新到旧累加时长，超出部分删除
  auto it = state.completed_segments.rbegin();
  while (it != state.completed_segments.rend()) {
    total_duration += it->duration_100ns;
    ++it;
  }

  while (!state.completed_segments.empty() && total_duration > max_duration_100ns) {
    auto& oldest = state.completed_segments.front();
    total_duration -= oldest.duration_100ns;

    // 删除文件
    std::error_code ec;
    std::filesystem::remove(oldest.path, ec);
    if (ec) {
      Logger().warn("Failed to remove old segment: {}", oldest.path.string());
    } else {
      Logger().debug("Removed old segment: {}", oldest.path.string());
    }

    state.completed_segments.pop_front();
  }
}

// 执行段落轮转（内部实现，调用方需持有 resource_mutex）
auto rotate_segment_internal(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string> {
  // 1. 记录当前段落的实际时长
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.current_segment.start_time;
  state.current_segment.duration_100ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

  // 2. Finalize 当前编码器（需要持有 encoder_write_mutex）
  {
    std::lock_guard write_lock(state.encoder_write_mutex);
    auto finalize_result = Utils::Media::Encoder::finalize_encoder(state.encoder);
    if (!finalize_result) {
      Logger().error("Failed to finalize segment: {}", finalize_result.error());
      // 继续执行，不要因为 finalize 失败阻断轮转
    }
  }

  // 3. 保存已完成段落信息
  state.completed_segments.push_back(state.current_segment);

  // 4. 创建新编码器
  auto result = create_new_segment(state);
  if (!result) {
    return std::unexpected("Failed to create new segment during rotation: " + result.error());
  }

  // 5. 清理过期段落
  cleanup_old_segments(state);

  Logger().debug("Segment rotated, {} completed segments in queue",
                 state.completed_segments.size());
  return {};
}

// 帧到达回调
auto on_frame_arrived(Features::ReplayBuffer::State::ReplayBufferState& state,
                      Utils::Graphics::Capture::Direct3D11CaptureFrame frame) -> void {
  if (state.status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.segment_start_time;
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

    std::expected<void, std::string> result;
    {
      std::lock_guard write_lock(state.encoder_write_mutex);
      result = Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(),
                                                   encode_texture, timestamp, state.config.fps);
    }

    if (!result) {
      Logger().error("Failed to encode frame {}: {}", state.frame_index, result.error());
      break;
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

  // 1. 创建临时目录
  auto temp_base = std::filesystem::temp_directory_path() / "SpinningMomo" / "replay_buffer";
  auto ensure_result = Utils::Path::EnsureDirectoryExists(temp_base);
  if (!ensure_result) {
    return std::unexpected("Failed to create temp directory: " + ensure_result.error());
  }
  state.temp_dir = temp_base;

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

  // 5. 创建首个编码器段落
  auto segment_result = create_new_segment(state);
  if (!segment_result) {
    return std::unexpected(segment_result.error());
  }

  // 6. 获取窗口尺寸
  RECT rect;
  GetClientRect(target_window, &rect);
  int width = (rect.right - rect.left) / 2 * 2;
  int height = (rect.bottom - rect.top) / 2 * 2;

  // 7. 创建捕获会话（2 帧缓冲）
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, width, height,
      [&state](auto frame) { on_frame_arrived(state, frame); }, 2);

  if (!capture_result) {
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 8. 启动捕获
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  // 9. 启动音频捕获线程
  if (state.encoder.has_audio) {
    Utils::Media::AudioCapture::start_capture_thread(
        state.audio,
        // get_elapsed_100ns: 相对于当前段落开始时间
        [&state]() -> std::int64_t {
          auto now = std::chrono::steady_clock::now();
          auto elapsed = now - state.segment_start_time;
          return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;
        },
        // is_active
        [&state]() -> bool {
          return state.status.load(std::memory_order_acquire) ==
                     Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering &&
                 state.encoder.has_audio;
        },
        // on_packet
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
              encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get());
            }
          }
        });
  }

  // 10. 启动轮转定时器
  if (!state.rotation_timer) {
    state.rotation_timer.emplace();
  }
  auto timer_interval = std::chrono::milliseconds(state.config.rotation_interval * 1000);
  auto timer_result = state.rotation_timer->SetTimer(timer_interval, [&state]() {
    std::lock_guard resource_lock(state.resource_mutex);
    if (state.status.load(std::memory_order_acquire) ==
        Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
      auto result = rotate_segment_internal(state);
      if (!result) {
        Logger().error("Segment rotation failed: {}", result.error());
      }

      // 重新设置定时器（Timer 是一次性的）
      auto interval = std::chrono::milliseconds(state.config.rotation_interval * 1000);
      (void)state.rotation_timer->SetTimer(interval, [&state]() {
        std::lock_guard rl(state.resource_mutex);
        if (state.status.load(std::memory_order_acquire) ==
            Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
          (void)rotate_segment_internal(state);
          // 继续轮转 — 递归设置（简化实现）
        }
      });
    }
  });

  if (!timer_result) {
    Logger().warn("Failed to set rotation timer, segments won't auto-rotate");
  }

  // 11. 设置状态为 Buffering
  state.status.store(Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering,
                     std::memory_order_release);

  Logger().info("ReplayBuffer started buffering");
  return {};
}

auto stop_buffering(Features::ReplayBuffer::State::ReplayBufferState& state) -> void {
  auto expected = Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::ReplayBuffer::Types::ReplayBufferStatus::Idle,
                                            std::memory_order_acq_rel)) {
    return;
  }

  // 1. 停止轮转定时器
  if (state.rotation_timer && state.rotation_timer->IsRunning()) {
    state.rotation_timer->Cancel();
  }

  // 2. 停止音频
  if (state.encoder.has_audio) {
    Utils::Media::AudioCapture::stop(state.audio);
  }

  // 3. 停止视频捕获
  Utils::Graphics::Capture::stop_capture(state.capture_session);

  // 4. 清理资源
  {
    std::lock_guard resource_lock(state.resource_mutex);

    auto finalize_result = Utils::Media::Encoder::finalize_encoder(state.encoder);
    if (!finalize_result) {
      Logger().error("Failed to finalize encoder: {}", finalize_result.error());
    }

    state.capture_session = {};
    state.encoder = {};
    state.last_encoded_texture = nullptr;
    state.device = nullptr;
    state.context = nullptr;

    Utils::Media::AudioCapture::cleanup(state.audio);
  }

  // 5. 清理所有临时段落文件
  for (auto& segment : state.completed_segments) {
    std::error_code ec;
    std::filesystem::remove(segment.path, ec);
  }
  state.completed_segments.clear();

  // 清理当前段落文件
  if (!state.current_segment.path.empty()) {
    std::error_code ec;
    std::filesystem::remove(state.current_segment.path, ec);
    state.current_segment = {};
  }

  Logger().info("ReplayBuffer stopped");
}

auto force_rotate(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string> {
  if (state.status.load(std::memory_order_acquire) !=
      Features::ReplayBuffer::Types::ReplayBufferStatus::Buffering) {
    return std::unexpected("ReplayBuffer is not buffering");
  }

  std::lock_guard resource_lock(state.resource_mutex);
  return rotate_segment_internal(state);
}

auto get_recent_segments(const Features::ReplayBuffer::State::ReplayBufferState& state,
                         double duration_seconds) -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> result;
  std::int64_t target_duration_100ns = static_cast<std::int64_t>(duration_seconds * 10'000'000);
  std::int64_t accumulated = 0;

  // 从最新的段落开始往回取
  for (auto it = state.completed_segments.rbegin(); it != state.completed_segments.rend(); ++it) {
    result.insert(result.begin(), it->path);
    accumulated += it->duration_100ns;
    if (accumulated >= target_duration_100ns) {
      break;
    }
  }

  return result;
}

auto cleanup(Features::ReplayBuffer::State::ReplayBufferState& state) -> void {
  stop_buffering(state);
  MFShutdown();
}

}  // namespace Features::ReplayBuffer
