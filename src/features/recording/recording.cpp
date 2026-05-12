module;

#include <wil/com.h>
#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording;

import std;
import Core.State;
import Features.Recording.State;
import Features.Recording.AudioCapture;
import UI.FloatingWindow;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Graphics.D3D;
import Utils.Media.Encoder;
import Utils.Media.Encoder.Types;
import Utils.Logger;
import Utils.String;
import <d3d11_4.h>;
import <dwmapi.h>;
import <mfapi.h>;
import <windows.h>;

namespace Features::Recording {

// 录制模块的大致数据流：
// WGC 帧回调只负责唤醒编码线程；音频采集线程只复制 PCM 数据并入队；
// 编码线程主动排空 WGC frame pool，并且是唯一写 SinkWriter 的地方；
// 控制线程负责把 start / stop / resize restart 串起来，避免多个重操作互相打架。
constexpr std::uint64_t k_discard_video_frame_threshold = 3;

auto floor_to_even(int value) -> int { return (value / 2) * 2; }
auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }

  constexpr long double kHundredNsPerSecond = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * kHundredNsPerSecond / frequency.QuadPart);
}

auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Features::Recording::State::RecordingState& state) -> void;
auto request_control_action(Features::Recording::State::RecordingState& state,
                            Features::Recording::State::RecordingControlAction action) -> bool;

auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                          int frame_height)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  if (frame_width <= 0 || frame_height <= 0) {
    return std::unexpected("Invalid frame size");
  }

  Features::Recording::State::CapturePlan plan;
  plan.source_width = frame_width;
  plan.source_height = frame_height;

  // 录整个窗口时基本不用算复杂裁剪，只要把宽高修成偶数。
  // 编码器通常不喜欢奇数宽高，尤其是 H.264/H.265。
  if (!capture_client_area) {
    auto output_width = floor_to_even(frame_width);
    auto output_height = floor_to_even(frame_height);
    if (output_width <= 0 || output_height <= 0) {
      return std::unexpected("Resolved full-window output size is invalid");
    }

    plan.output_width = static_cast<std::uint32_t>(output_width);
    plan.output_height = static_cast<std::uint32_t>(output_height);
    plan.should_crop = output_width != frame_width || output_height != frame_height;
    plan.region = {
        .left = 0,
        .top = 0,
        .width = static_cast<UINT>(output_width),
        .height = static_cast<UINT>(output_height),
    };
    return plan;
  }

  // 只录客户区时，WGC 给的是完整窗口画面，这里要把边框和标题栏裁掉。
  auto crop_region_result = Utils::Graphics::CaptureRegion::calculate_client_crop_region(
      target_window, static_cast<UINT>(frame_width), static_cast<UINT>(frame_height));
  if (!crop_region_result) {
    return std::unexpected("Failed to calculate client crop region: " + crop_region_result.error());
  }

  auto region = *crop_region_result;
  auto output_width = floor_to_even(static_cast<int>(region.width));
  auto output_height = floor_to_even(static_cast<int>(region.height));
  if (output_width <= 0 || output_height <= 0) {
    return std::unexpected("Resolved client-area output size is invalid");
  }

  region.width = static_cast<UINT>(output_width);
  region.height = static_cast<UINT>(output_height);

  plan.output_width = static_cast<std::uint32_t>(output_width);
  plan.output_height = static_cast<std::uint32_t>(output_height);
  plan.should_crop = region.left != 0 || region.top != 0 || output_width != frame_width ||
                     output_height != frame_height;
  plan.region = region;
  return plan;
}

auto calculate_frame_crop_plan(HWND target_window,
                               const Features::Recording::Types::RecordingConfig& config,
                               int frame_width, int frame_height)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  auto capture_plan_result =
      resolve_capture_plan(target_window, config.capture_client_area, frame_width, frame_height);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  // 正常录制中，输出尺寸应该和启动时定下来的尺寸一致。
  // 如果窗口尺寸真的变了，交给 auto restart 切一个新文件，而不是硬塞进老编码器。
  if (capture_plan_result->output_width != config.width ||
      capture_plan_result->output_height != config.height) {
    return std::unexpected(std::format("Unexpected recording output size {}x{} (expected {}x{})",
                                       capture_plan_result->output_width,
                                       capture_plan_result->output_height, config.width,
                                       config.height));
  }

  return capture_plan_result;
}

auto build_working_output_path(const std::filesystem::path& final_output_path)
    -> std::filesystem::path {
  auto working_output_path = final_output_path;
  // 录制没成功封尾前不直接写 .mp4，避免图库或播放器误以为这是一个完整视频。
  // 例如 final 是 20260511_xxx.mp4，这里会先写到 20260511_xxx。
  working_output_path.replace_extension();
  return working_output_path;
}

auto rename_working_output_to_final(const std::filesystem::path& working_output_path,
                                    const std::filesystem::path& final_output_path)
    -> std::expected<void, std::string> {
  if (working_output_path.empty() || final_output_path.empty() ||
      working_output_path == final_output_path) {
    return {};
  }

  std::error_code ec;
  if (std::filesystem::exists(final_output_path, ec)) {
    if (ec) {
      return std::unexpected("Failed to probe final output path: " + ec.message());
    }

    std::filesystem::remove(final_output_path, ec);
    if (ec) {
      return std::unexpected("Failed to remove existing final output file: " + ec.message());
    }
  }

  // 只有编码线程 finalize 成功后才走到这里。
  // rename 比复制更快，也能减少半成品 .mp4 被看到的时间窗口。
  std::filesystem::rename(working_output_path, final_output_path, ec);
  if (ec) {
    return std::unexpected("Failed to move finalized recording to destination: " + ec.message());
  }

  return {};
}

auto delete_working_output_file(const std::filesystem::path& working_output_path,
                                std::string_view reason) -> void {
  if (working_output_path.empty()) {
    return;
  }

  std::error_code ec;
  if (!std::filesystem::exists(working_output_path, ec)) {
    if (ec) {
      Logger().warn("Failed to probe discarded recording file '{}': {}",
                    working_output_path.string(), ec.message());
    }
    return;
  }

  std::filesystem::remove(working_output_path, ec);
  if (ec) {
    Logger().warn("Failed to delete discarded recording file '{}': {}",
                  working_output_path.string(), ec.message());
    return;
  }

  Logger().info("Discarded recording file '{}': {}", working_output_path.string(), reason);
}

auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  // 启动时以 WGC 的真实尺寸为准，不用窗口矩形猜。
  // 窗口边框、DPI、奇偶像素都可能让窗口矩形和实际帧差 1px。
  auto capture_size_result = Utils::Graphics::Capture::get_capture_item_size(target_window);
  if (!capture_size_result) {
    return std::unexpected(capture_size_result.error());
  }

  return resolve_capture_plan(target_window, capture_client_area, capture_size_result->first,
                              capture_size_result->second);
}

auto build_timestamp_output_path(const std::filesystem::path& reference_output_path)
    -> std::filesystem::path {
  auto filename = Utils::String::FormatTimestamp(std::chrono::system_clock::now());
  auto dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos) + ".mp4";
  } else {
    filename += ".mp4";
  }
  return reference_output_path.parent_path() / filename;
}

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

auto clear_queues(State::RecordingState& state) -> void {
  // 队列和通知标志只能在 queue_mutex 下动。音频线程、WGC 回调、编码线程都会碰它。
  std::lock_guard queue_lock(state.queue_mutex);
  state.audio_queue.clear();
  state.video_frame_pending = false;
}

auto clear_session_runtime_fields(State::RecordingState& state) -> void {
  // 清空一个录制段的会话态，不动可复用 D3D 设备。
  state.config = {};
  state.working_output_path.clear();
  state.target_window = nullptr;
  state.capture_plan = {};
  state.capture_session = {};
  state.cropped_texture = nullptr;
  state.encoder = {};
  state.start_qpc_100ns = 0;
  state.video_timeline = {};
  state.last_frame_width = 0;
  state.last_frame_height = 0;
  clear_queues(state);
  state.dropped_audio_packets.store(0, std::memory_order_release);
  state.encoded_video_frames = 0;
  state.encoded_audio_packets = 0;
  state.encoder_thread = {};
  state.accepting_input.store(false, std::memory_order_release);
  state.finish_requested.store(false, std::memory_order_release);
  state.has_audio.store(false, std::memory_order_release);
  state.encoder_ready = false;
  state.encoder_start_succeeded = false;
  state.finalize_succeeded = false;
  state.encoder_error.clear();
}

auto clear_persistent_runtime_fields(State::RecordingState& state) -> void {
  state.winrt_device = nullptr;
  state.context = nullptr;
  state.device = nullptr;
  state.d3d_initialized = false;
}

auto notify_encoder_ready(State::RecordingState& state, bool succeeded, std::string error = {})
    -> void {
  // 编码器在编码线程里创建。start() 会等这个信号，确认能不能真正开始捕获。
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

auto wait_encoder_ready(State::RecordingState& state) -> void {
  std::unique_lock ready_lock(state.encoder_ready_mutex);
  state.encoder_ready_cv.wait(ready_lock, [&state]() { return state.encoder_ready; });
}

auto cancel_cleanup_timer(State::RecordingState& state) -> void {
  if (state.cleanup_timer && state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
  }
}

auto start_cleanup_timer(State::RecordingState& state) -> void {
  if (!state.d3d_initialized) {
    return;
  }

  if (!state.cleanup_timer) {
    state.cleanup_timer.emplace();
  }

  cancel_cleanup_timer(state);
  auto result = state.cleanup_timer->set_timeout(std::chrono::milliseconds(5000), [&state]() {
    request_control_action(state, State::RecordingControlAction::CleanupD3D);
  });
  if (!result) {
    Logger().warn("Failed to set recording D3D cleanup timer");
    return;
  }

  Logger().debug("Recording D3D cleanup timer started (5 seconds)");
}

auto cleanup_d3d_resources(State::RecordingState& state) -> void {
  cancel_cleanup_timer(state);
  clear_persistent_runtime_fields(state);
}

auto ensure_d3d_resources_ready(State::RecordingState& state) -> std::expected<void, std::string> {
  if (state.d3d_initialized && state.device && state.context && state.winrt_device) {
    return {};
  }

  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    cleanup_d3d_resources(state);
    return std::unexpected("Failed to create D3D device: " + d3d_result.error());
  }
  state.device = d3d_result->first;
  state.context = d3d_result->second;

  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(state.device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    multithread->SetMultithreadProtected(TRUE);
  }

  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(state.device.get());
  if (!winrt_device_result) {
    cleanup_d3d_resources(state);
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
  }

  state.winrt_device = *winrt_device_result;
  state.d3d_initialized = true;
  return {};
}

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

auto consume_video_frame_pending(State::RecordingState& state) -> bool {
  std::lock_guard queue_lock(state.queue_mutex);
  const bool pending = state.video_frame_pending;
  state.video_frame_pending = false;
  return pending;
}

auto pop_next_audio_packet(State::RecordingState& state)
    -> std::optional<State::QueuedAudioPacket> {
  std::lock_guard queue_lock(state.queue_mutex);
  if (state.audio_queue.empty()) {
    return std::nullopt;
  }

  auto packet = std::move(state.audio_queue.front());
  state.audio_queue.pop_front();
  return packet;
}

auto has_pending_encoder_work(State::RecordingState& state) -> bool {
  std::lock_guard queue_lock(state.queue_mutex);
  return state.video_frame_pending || !state.audio_queue.empty();
}

auto wait_for_encoder_work(State::RecordingState& state, std::stop_token stop_token) -> void {
  std::unique_lock queue_lock(state.queue_mutex);
  // 编码线程平时睡在这里；有新视频帧通知、新音频，或 stop 要求收尾时被叫醒。
  state.queue_cv.wait(queue_lock, [&state, stop_token]() {
    return stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire) ||
           state.video_frame_pending || !state.audio_queue.empty();
  });
}

auto write_audio_packet(Utils::Media::Encoder::State::EncoderContext& encoder,
                        const State::QueuedAudioPacket& packet)
    -> std::expected<void, std::string> {
  if (!encoder.sink_writer || !encoder.has_audio || packet.data.empty()) {
    return {};
  }

  // 音频线程只给我们原始字节；写入 SinkWriter 前要包装成 Media Foundation 的 sample。
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

// WGC 的 SystemRelativeTime 使用 QPC 时间基，WASAPI 的 qpc_position 也是同一时间基。
// 这里减去录制开始时的 QPC，把视频帧时间转成“本段录制从 0 开始”的 100ns 时间戳。
auto get_frame_timestamp_100ns(State::RecordingState& state,
                               Utils::Graphics::Capture::Direct3D11CaptureFrame frame)
    -> std::int64_t {
  if (state.start_qpc_100ns <= 0) {
    return 0;
  }

  return std::max<std::int64_t>(0, frame.SystemRelativeTime().count() - state.start_qpc_100ns);
}

auto video_frame_interval_100ns(const State::RecordingState& state) -> std::int64_t {
  const auto fps = std::max<std::uint32_t>(state.config.fps, 1);
  return std::max<std::int64_t>(1, 10'000'000LL / fps);
}

auto mark_missing_video_until(State::RecordingState& state, std::int64_t target_timestamp_100ns)
    -> std::expected<void, std::string> {
  if (!state.encoder.sink_writer || target_timestamp_100ns < 0) {
    return {};
  }

  const auto frame_interval_100ns = video_frame_interval_100ns(state);
  const auto half_frame_100ns = frame_interval_100ns / 2;

  // 这里不生成重复画面，只向 MF 声明 video stream 在这些帧点没有 sample。
  // 这样音频继续写入时，SinkWriter 不会看到“音频时间线前进，视频流毫无交代地停住”。
  // 保留半帧容忍，避免把正常调度抖动误判成缺帧，并避免 tick 紧贴真实帧。
  while (state.video_timeline.next_expected_timestamp_100ns + half_frame_100ns <=
         target_timestamp_100ns) {
    const auto tick_timestamp = state.video_timeline.next_expected_timestamp_100ns;
    HRESULT hr =
        state.encoder.sink_writer->SendStreamTick(state.encoder.video_stream_index, tick_timestamp);
    if (FAILED(hr)) {
      return std::unexpected(
          std::format("Failed to send video stream tick at {} (HRESULT: 0x{:08X})", tick_timestamp,
                      static_cast<unsigned>(hr)));
    }

    state.video_timeline.next_expected_timestamp_100ns += frame_interval_100ns;
    state.video_timeline.ticks_sent++;
    state.video_timeline.sample_after_gap = true;
  }

  return {};
}

auto commit_video_sample_time(State::RecordingState& state, std::int64_t timestamp_100ns) -> void {
  // 真实视频 sample 已经覆盖 timestamp_100ns，把“下一帧应出现时间”推进到后一帧。
  // 如果前面发过 tick，这个 sample 写入成功后 gap 已结束。
  const auto next_timestamp = timestamp_100ns + video_frame_interval_100ns(state);
  if (next_timestamp > state.video_timeline.next_expected_timestamp_100ns) {
    state.video_timeline.next_expected_timestamp_100ns = next_timestamp;
  }
  state.video_timeline.sample_after_gap = false;
}

auto encode_capture_frame(State::RecordingState& state,
                          Utils::Graphics::Capture::Direct3D11CaptureFrame frame)
    -> std::expected<void, std::string> {
  if (!frame) {
    return {};
  }

  auto content_size = frame.ContentSize();
  if (content_size.Width > 0 && content_size.Height > 0) {
    bool frame_size_changed = content_size.Width != state.last_frame_width ||
                              content_size.Height != state.last_frame_height;

    if (frame_size_changed) {
      Utils::Graphics::Capture::recreate_frame_pool(state.capture_session, content_size.Width,
                                                    content_size.Height);

      auto capture_plan_result =
          resolve_capture_plan(state.target_window, state.config.capture_client_area,
                               content_size.Width, content_size.Height);
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
        request_restart_after_resize(state);
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
    auto capture_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                         static_cast<int>(source_desc.Width),
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
    auto capture_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                         static_cast<int>(source_desc.Width),
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

  const auto timestamp_100ns = get_frame_timestamp_100ns(state, frame);
  if (state.encoded_video_frames > 0) {
    // 两个真实 WGC 帧之间可能隔了很久；先把中间缺失的视频帧标成 stream tick。
    auto tick_result = mark_missing_video_until(state, timestamp_100ns);
    if (!tick_result) {
      return tick_result;
    }
  }

  const bool discontinuity = state.video_timeline.sample_after_gap;
  auto result =
      Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(), current_texture,
                                          timestamp_100ns, state.config.fps, discontinuity);
  if (!result) {
    return result;
  }

  state.encoded_video_frames++;
  commit_video_sample_time(state, timestamp_100ns);
  return {};
}

auto encode_available_video_frames(State::RecordingState& state)
    -> std::expected<void, std::string> {
  std::lock_guard frame_lock(state.frame_mutex);

  while (auto frame = Utils::Graphics::Capture::try_get_next_frame(state.capture_session)) {
    auto result = encode_capture_frame(state, frame);
    if (!result) {
      return result;
    }
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

auto encoder_thread_proc(State::RecordingState& state, std::stop_token stop_token) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // SinkWriter 在这个线程创建，也只在这个线程 WriteSample / Finalize。
    // 这样可以避开 Media Foundation 编码器跨线程调用时的驱动兼容性问题。
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

    // 主循环：视频从 WGC frame pool 主动拉取，音频从队列取包，然后写给编码器。
    // stop 时不会立刻丢掉已到达的数据，而是先排空再 finalize。
    while (true) {
      wait_for_encoder_work(state, stop_token);

      const bool finishing =
          stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire);

      if (consume_video_frame_pending(state) || finishing) {
        auto video_result = encode_available_video_frames(state);
        if (!video_result) {
          state.encoder_error = video_result.error();
          Logger().error("Recording encoder failed: {}", video_result.error());
          break;
        }
      }

      while (auto audio_packet = pop_next_audio_packet(state)) {
        // 音频可能在 WGC 停帧期间继续推进。写音频前先补 video tick，
        // 避免 SinkWriter 内部等待一个一直没有到来的 video sample。
        auto tick_result = mark_missing_video_until(state, audio_packet->timestamp_100ns);
        if (!tick_result) {
          state.encoder_error = tick_result.error();
          Logger().error("Recording encoder failed: {}", tick_result.error());
          break;
        }

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

      if (finishing && !has_pending_encoder_work(state)) {
        break;
      }
    }

    // 少于几个视频帧的片段大概率是误触或启动失败，直接丢弃。
    // 真正可用的片段才执行 finalize，finalize 成功后外层 stop 再改名成 .mp4。
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

auto request_control_action(Features::Recording::State::RecordingState& state,
                            Features::Recording::State::RecordingControlAction action) -> bool {
  if (state.shutdown_requested.load(std::memory_order_acquire) &&
      action != State::RecordingControlAction::ShutdownStop) {
    return false;
  }

  {
    std::lock_guard request_lock(state.control_request_mutex);

    // shutdown 优先级最高。退出时不再接受普通 toggle / resize restart。
    if (action == State::RecordingControlAction::ShutdownStop) {
      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // 用户快速连按时，只保留一个 toggle。
    // 如果当前已经在 start/stop，新的 toggle 直接忽略，避免重入。
    if (action == State::RecordingControlAction::Toggle) {
      if (state.control_action_running.load(std::memory_order_acquire) ||
          state.pending_action != State::RecordingControlAction::None) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // resize restart 可以被后来的 resize 覆盖，但不能插到 shutdown 或用户 toggle 前面。
    if (action == State::RecordingControlAction::RestartAfterResize) {
      if (state.pending_action == State::RecordingControlAction::ShutdownStop ||
          state.pending_action == State::RecordingControlAction::Toggle) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    if (action == State::RecordingControlAction::CleanupD3D) {
      if (state.pending_action == State::RecordingControlAction::ShutdownStop ||
          state.pending_action == State::RecordingControlAction::Toggle ||
          state.pending_action == State::RecordingControlAction::RestartAfterResize) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }
  }

  return false;
}

auto handle_control_action(Core::State::AppState& app_state,
                           Features::Recording::State::RecordingState& state,
                           const RecordingControlHandlers& handlers,
                           Features::Recording::State::RecordingControlAction action) -> bool {
  switch (action) {
    case State::RecordingControlAction::Toggle:
      if (handlers.on_toggle) {
        handlers.on_toggle();
      }
      return true;

    case State::RecordingControlAction::RestartAfterResize:
      restart_after_resize(app_state, state);
      return true;

    case State::RecordingControlAction::ShutdownStop:
      if (handlers.on_shutdown_stop) {
        handlers.on_shutdown_stop();
      }
      return false;

    case State::RecordingControlAction::CleanupD3D:
      if (state.status.load(std::memory_order_acquire) ==
          Features::Recording::Types::RecordingStatus::Idle) {
        cleanup_d3d_resources(state);
        Logger().debug("Recording reusable D3D resources cleaned up");
      }
      return true;

    default:
      return true;
  }
}

auto control_thread_proc(Core::State::AppState& app_state,
                         Features::Recording::State::RecordingState& state,
                         RecordingControlHandlers handlers, std::stop_token stop_token) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    std::stop_callback wake_on_stop(stop_token, [&state]() { state.control_cv.notify_all(); });

    while (true) {
      State::RecordingControlAction action{State::RecordingControlAction::None};

      {
        std::unique_lock request_lock(state.control_request_mutex);
        // 控制线程平时睡在这里。有 toggle / resize / shutdown 请求时醒来处理。
        state.control_cv.wait(request_lock, [&]() {
          return stop_token.stop_requested() ||
                 state.pending_action != State::RecordingControlAction::None;
        });

        if (stop_token.stop_requested() &&
            state.pending_action == State::RecordingControlAction::None) {
          break;
        }

        // 取出请求后立刻清空槽位。真正执行 start/stop 时不拿这个锁，
        // 否则 UI 线程提交 shutdown 请求可能被长时间卡住。
        action = state.pending_action;
        state.pending_action = State::RecordingControlAction::None;
        state.control_action_running.store(true, std::memory_order_release);
      }

      bool keep_running = true;
      try {
        keep_running = handle_control_action(app_state, state, handlers, action);
      } catch (const std::exception& e) {
        Logger().error("Recording control thread action exception: {}", e.what());
      } catch (...) {
        Logger().error("Recording control thread action exception: unknown");
      }

      state.control_action_running.store(false, std::memory_order_release);
      if (!keep_running) {
        break;
      }
    }
  } catch (const wil::ResultException& e) {
    Logger().error("Recording control thread COM initialization failed: {} (HRESULT: 0x{:08X})",
                   e.what(), static_cast<unsigned>(e.GetErrorCode()));
    state.control_action_running.store(false, std::memory_order_release);
  } catch (const std::exception& e) {
    Logger().error("Recording control thread exception: {}", e.what());
    state.control_action_running.store(false, std::memory_order_release);
  } catch (...) {
    Logger().error("Recording control thread exception: unknown");
    state.control_action_running.store(false, std::memory_order_release);
  }
}

auto ensure_control_thread_started(Core::State::AppState& app_state,
                                   Features::Recording::State::RecordingState& state,
                                   RecordingControlHandlers handlers)
    -> std::expected<void, std::string> {
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  if (state.control_thread.joinable()) {
    return {};
  }

  state.control_thread = std::jthread(
      [&app_state, &state, handlers = std::move(handlers)](std::stop_token stop_token) mutable {
        control_thread_proc(app_state, state, std::move(handlers), stop_token);
      });
  return {};
}

auto join_control_thread(Features::Recording::State::RecordingState& state) -> void {
  if (state.control_thread.joinable()) {
    state.control_thread.join();
  }
}

auto request_restart_after_resize(Features::Recording::State::RecordingState& state) -> void {
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  request_control_action(state, State::RecordingControlAction::RestartAfterResize);
}

auto restart_after_resize(Core::State::AppState& app_state,
                          Features::Recording::State::RecordingState& state) -> void {
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  if (state.status.load(std::memory_order_acquire) !=
      Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto restart_config = state.config;
  auto target_window = state.target_window;

  if (!target_window || !IsWindow(target_window)) {
    Logger().error("Skip recording auto restart after resize: target window is invalid");
    return;
  }

  // 尺寸变化后不能继续写进旧编码器，所以保存当前段，再用新尺寸开一个新段。
  restart_config.output_path = build_timestamp_output_path(restart_config.output_path);
  Logger().info("Recording restarted with timestamp output after resize: {}",
                restart_config.output_path.string());

  stop(state);
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  auto restart_result = start(app_state, state, target_window, restart_config);
  if (!restart_result) {
    Logger().error("Failed to restart recording after resize: {}", restart_result.error());
  } else {
    UI::FloatingWindow::request_repaint(app_state);
  }
}

auto initialize(Features::Recording::State::RecordingState& state)
    -> std::expected<void, std::string> {
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(Features::Recording::State::RecordingState& state) -> void {
  mark_video_frame_pending(state);
}

auto signal_encoder_finish(State::RecordingState& state) -> void {
  {
    std::lock_guard queue_lock(state.queue_mutex);
    // 告诉编码线程：不会再有新数据了，把音频队列和 WGC 帧池里剩下的数据写完就收尾。
    state.finish_requested.store(true, std::memory_order_release);
    state.video_frame_pending = true;
  }
  state.queue_cv.notify_all();
}

auto stop_capture_input(State::RecordingState& state) -> void {
  // 编码线程可能正在从 frame pool 取帧并直接写 SinkWriter；先互斥地停止产帧。
  std::lock_guard frame_lock(state.frame_mutex);
  Utils::Graphics::Capture::stop_capture_session(state.capture_session);
}

auto cleanup_failed_start(State::RecordingState& state, std::string_view reason) -> void {
  // start 中途失败也走一遍“停输入 -> 停止产帧 -> 停编码线程 -> 清资源”。
  // 这样失败路径和正常 stop 的资源顺序保持一致。
  state.accepting_input.store(false, std::memory_order_release);
  stop_capture_input(state);
  Features::Recording::AudioCapture::stop(state.audio);
  signal_encoder_finish(state);

  if (state.encoder_thread.joinable()) {
    state.encoder_thread.request_stop();
    state.encoder_thread.join();
  }

  Features::Recording::AudioCapture::cleanup(state.audio);
  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);
  delete_working_output_file(state.working_output_path, reason);
  clear_session_runtime_fields(state);
}

auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  (void)app_state;

  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  clear_session_runtime_fields(state);
  cancel_cleanup_timer(state);

  // 先算清楚这次录制的源尺寸和输出尺寸。编码器创建后，宽高就不能再改。
  auto capture_plan_result = build_startup_capture_plan(target_window, config.capture_client_area);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  state.config = config;
  state.target_window = target_window;
  state.working_output_path = build_working_output_path(config.output_path);
  state.capture_plan = *capture_plan_result;
  state.last_frame_width = capture_plan_result->source_width;
  state.last_frame_height = capture_plan_result->source_height;
  state.config.width = static_cast<std::uint32_t>(capture_plan_result->output_width);
  state.config.height = static_cast<std::uint32_t>(capture_plan_result->output_height);

  // 录制内复用 D3D 设备，避免高频启停反复初始化。
  auto d3d_ready_result = ensure_d3d_resources_ready(state);
  if (!d3d_ready_result) {
    clear_session_runtime_fields(state);
    return d3d_ready_result;
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  auto audio_result =
      Features::Recording::AudioCapture::initialize(state.audio, config.audio_source, process_id);
  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    Logger().info("Audio capture initialized");
  }

  // 先启动编码线程并等它创建好 SinkWriter，再开始 WGC 捕获。
  // 否则捕获已经在产帧，但编码器可能还没准备好。
  state.encoder_thread = std::jthread(
      [&state](std::stop_token stop_token) { encoder_thread_proc(state, stop_token); });
  wait_encoder_ready(state);

  if (!state.encoder_start_succeeded) {
    std::string error =
        state.encoder_error.empty() ? "Failed to start recording encoder" : state.encoder_error;
    cleanup_failed_start(state, "recording start failed");
    return std::unexpected(error);
  }

  Utils::Graphics::Capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;

  // WGC 回调只通知编码线程；真正取帧和写编码器都在编码线程里完成。
  auto capture_result = Utils::Graphics::Capture::create_capture_session_with_frame_notification(
      target_window, state.winrt_device, state.capture_plan.source_width,
      state.capture_plan.source_height, [&state]() { on_frame_arrived(state); }, 3,
      capture_options);

  if (!capture_result) {
    cleanup_failed_start(state, "recording start failed");
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  state.start_qpc_100ns = query_qpc_100ns();
  state.accepting_input.store(true, std::memory_order_release);

  // 从这里开始，WGC 可能随时通知 on_frame_arrived。
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    cleanup_failed_start(state, "recording start failed");
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  state.status.store(Features::Recording::Types::RecordingStatus::Recording,
                     std::memory_order_release);

  if (state.has_audio.load(std::memory_order_acquire)) {
    Features::Recording::AudioCapture::start_capture_thread(state);
  }

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

auto stop(Features::Recording::State::RecordingState& state) -> void {
  auto expected = Features::Recording::Types::RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::Recording::Types::RecordingStatus::Stopping,
                                            std::memory_order_acq_rel)) {
    return;
  }

  // 先关输入入口。已经进入队列的数据会继续写完，新来的帧/音频会被拒绝。
  state.accepting_input.store(false, std::memory_order_release);

  // 先停止 WGC 继续产帧，但保留 frame pool，让编码线程排空已经到达的帧。
  auto stop_capture_start = std::chrono::steady_clock::now();
  stop_capture_input(state);
  Logger().debug("Recording capture stopped in {}ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - stop_capture_start)
                     .count());

  if (state.has_audio.load(std::memory_order_acquire)) {
    auto stop_audio_start = std::chrono::steady_clock::now();
    Features::Recording::AudioCapture::stop(state.audio);
    Logger().debug("Recording audio stopped in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - stop_audio_start)
                       .count());
  }

  // 通知编码线程收尾。它会把队列里剩下的数据写完，然后 finalize。
  signal_encoder_finish(state);

  // 不 detach。stop 等编码线程自然结束，这样 MF/SinkWriter 的生命周期是明确的。
  if (state.encoder_thread.joinable()) {
    state.encoder_thread.join();
  }

  Features::Recording::AudioCapture::cleanup(state.audio);
  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);

  // finalize 成功才把无扩展名临时文件改成 .mp4；否则删除临时文件。
  if (state.finalize_succeeded) {
    auto rename_result =
        rename_working_output_to_final(state.working_output_path, state.config.output_path);
    if (!rename_result) {
      Logger().error("Failed to publish finalized recording '{}': {}",
                     state.config.output_path.string(), rename_result.error());
    }
  } else {
    auto reason = state.encoder_error.empty() ? "too few video frames or finalize skipped"
                                              : state.encoder_error;
    delete_working_output_file(state.working_output_path, reason);
  }

  auto dropped_audio = state.dropped_audio_packets.load(std::memory_order_relaxed);
  if (dropped_audio > 0) {
    Logger().warn("Recording queue dropped audio packets: {}", dropped_audio);
  }
  if (state.video_timeline.ticks_sent > 0) {
    Logger().info("Recording sent video stream ticks: {}", state.video_timeline.ticks_sent);
  }

  clear_session_runtime_fields(state);
  state.status.store(Features::Recording::Types::RecordingStatus::Idle, std::memory_order_release);
  start_cleanup_timer(state);
  Logger().info("Recording stopped");
}

auto cleanup(Features::Recording::State::RecordingState& state) -> void {
  stop(state);
  cleanup_d3d_resources(state);
  MFShutdown();
}

}  // namespace Features::Recording
