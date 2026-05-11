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
// WGC 帧回调 / 音频采集线程只负责“把数据复制出来并入队”；
// 编码线程是唯一写 SinkWriter 的地方；
// 控制线程负责把 start / stop / resize restart 串起来，避免多个重操作互相打架。
constexpr std::uint64_t k_discard_video_frame_threshold = 3;

enum class QueuedItemKind {
  None,
  Video,
  Audio,
};

struct QueuedItem {
  QueuedItemKind kind{QueuedItemKind::None};
  State::QueuedVideoFrame video;
  State::QueuedAudioPacket audio;
};

auto floor_to_even(int value) -> int { return (value / 2) * 2; }
auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Features::Recording::State::RecordingState& state) -> void;

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
  // 队列只能在 queue_mutex 下动。视频回调、音频线程、编码线程都会碰它。
  std::lock_guard queue_lock(state.queue_mutex);
  state.video_queue.clear();
  state.audio_queue.clear();
}

auto clear_runtime_fields(State::RecordingState& state) -> void {
  // 完整清空一个录制段的运行态。调用前必须保证相关线程已经停好或尚未启动。
  state.config = {};
  state.working_output_path.clear();
  state.target_window = nullptr;
  state.capture_plan = {};
  state.device = nullptr;
  state.context = nullptr;
  state.capture_session = {};
  state.cropped_texture = nullptr;
  state.encoder = {};
  state.start_time = {};
  state.last_frame_width = 0;
  state.last_frame_height = 0;
  clear_queues(state);
  state.dropped_video_frames.store(0, std::memory_order_release);
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

auto pop_next_queued_item(State::RecordingState& state) -> QueuedItem {
  QueuedItem item;

  if (state.video_queue.empty() && state.audio_queue.empty()) {
    return item;
  }

  // 音频和视频是两个生产者，入队时间可能交错。
  // 这里按时间戳取更早的一个，让写入顺序尽量接近真实时间线。
  if (!state.video_queue.empty() && !state.audio_queue.empty()) {
    if (state.video_queue.front().timestamp_100ns <= state.audio_queue.front().timestamp_100ns) {
      item.kind = QueuedItemKind::Video;
      item.video = std::move(state.video_queue.front());
      state.video_queue.pop_front();
    } else {
      item.kind = QueuedItemKind::Audio;
      item.audio = std::move(state.audio_queue.front());
      state.audio_queue.pop_front();
    }
    return item;
  }

  if (!state.video_queue.empty()) {
    item.kind = QueuedItemKind::Video;
    item.video = std::move(state.video_queue.front());
    state.video_queue.pop_front();
    return item;
  }

  item.kind = QueuedItemKind::Audio;
  item.audio = std::move(state.audio_queue.front());
  state.audio_queue.pop_front();
  return item;
}

auto wait_next_queued_item(State::RecordingState& state, std::stop_token stop_token) -> QueuedItem {
  std::unique_lock queue_lock(state.queue_mutex);
  // 编码线程平时睡在这里；有新帧、新音频，或 stop 要求收尾时被叫醒。
  state.queue_cv.wait(queue_lock, [&state, stop_token]() {
    return stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire) ||
           !state.video_queue.empty() || !state.audio_queue.empty();
  });

  if (state.video_queue.empty() && state.audio_queue.empty()) {
    return {};
  }

  return pop_next_queued_item(state);
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

auto encode_queued_video(State::RecordingState& state, State::QueuedVideoFrame& frame)
    -> std::expected<void, std::string> {
  auto result =
      Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(), frame.texture.get(),
                                          frame.timestamp_100ns, state.config.fps);
  if (!result) {
    return result;
  }

  state.encoded_video_frames++;
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

    // 主循环：不断从队列取视频帧或音频包，写给编码器。
    // stop 时不会立刻丢掉队列，而是先把已经排队的数据写完，再 finalize。
    while (true) {
      auto item = wait_next_queued_item(state, stop_token);
      if (item.kind == QueuedItemKind::None) {
        if (stop_token.stop_requested() || state.finish_requested.load(std::memory_order_acquire)) {
          break;
        }
        continue;
      }

      std::expected<void, std::string> encode_result;
      if (item.kind == QueuedItemKind::Video) {
        encode_result = encode_queued_video(state, item.video);
      } else {
        encode_result = encode_queued_audio(state, item.audio);
      }

      if (!encode_result) {
        state.encoder_error = encode_result.error();
        Logger().error("Recording encoder failed: {}", encode_result.error());
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

auto enqueue_video_frame(State::RecordingState& state, State::QueuedVideoFrame frame) -> void {
  {
    std::lock_guard queue_lock(state.queue_mutex);
    if (!state.accepting_input.load(std::memory_order_acquire)) {
      return;
    }

    // 队列满了就丢最旧的视频帧，优先保证录制继续。
    // 这里不阻塞 WGC 回调，否则帧池可能被堵住，反而更容易卡。
    if (state.video_queue.size() >= State::k_max_video_queue_size) {
      state.video_queue.pop_front();
      state.dropped_video_frames.fetch_add(1, std::memory_order_relaxed);
    }

    state.video_queue.push_back(std::move(frame));
  }
  state.queue_cv.notify_one();
}

auto copy_texture_for_queue(State::RecordingState& state, ID3D11Texture2D* texture)
    -> std::expected<wil::com_ptr<ID3D11Texture2D>, std::string> {
  if (!texture) {
    return std::unexpected("Recording frame texture is null");
  }

  D3D11_TEXTURE2D_DESC desc{};
  texture->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = 0;

  // WGC 的 frame 对象出了回调就不能继续依赖，所以这里复制一份纹理给编码线程慢慢用。
  wil::com_ptr<ID3D11Texture2D> copied_texture;
  HRESULT hr = state.device->CreateTexture2D(&desc, nullptr, copied_texture.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format(
        "Failed to create recording queue texture (HRESULT: 0x{:08X})", static_cast<unsigned>(hr)));
  }

  state.context->CopyResource(copied_texture.get(), texture);
  return copied_texture;
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
  clear_runtime_fields(state);
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(Features::Recording::State::RecordingState& state,
                      Utils::Graphics::Capture::Direct3D11CaptureFrame frame) -> void {
  // stop 已经开始后，新来的帧直接不要了。
  if (!state.accepting_input.load(std::memory_order_acquire) ||
      state.status.load(std::memory_order_acquire) !=
          Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.start_time;
  auto timestamp_100ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

  // 这里的锁不是为了“加速”，而是为了 stop/reset 时别把 D3D/WGC 资源清掉。
  // stop_capture 后会拿同一把锁等这里退出。
  std::lock_guard frame_lock(state.frame_mutex);
  if (!state.accepting_input.load(std::memory_order_acquire) ||
      state.status.load(std::memory_order_acquire) !=
          Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto content_size = frame.ContentSize();
  if (content_size.Width > 0 && content_size.Height > 0) {
    bool frame_size_changed = content_size.Width != state.last_frame_width ||
                              content_size.Height != state.last_frame_height;

    if (frame_size_changed) {
      // WGC 源尺寸变了，帧池也要跟着重建，否则后续拿到的帧尺寸会不对。
      Utils::Graphics::Capture::recreate_frame_pool(state.capture_session, content_size.Width,
                                                    content_size.Height);

      auto capture_plan_result =
          resolve_capture_plan(state.target_window, state.config.capture_client_area,
                               content_size.Width, content_size.Height);
      if (!capture_plan_result) {
        Logger().error("Failed to resolve recording crop plan after resize: {}",
                       capture_plan_result.error());
        return;
      }

      state.last_frame_width = content_size.Width;
      state.last_frame_height = content_size.Height;

      if (state.config.auto_restart_on_resize &&
          (capture_plan_result->output_width != state.config.width ||
           capture_plan_result->output_height != state.config.height)) {
        // 编码器的宽高启动后不能改。这里不直接 stop/start，只提交给控制线程处理。
        request_restart_after_resize(state);
        return;
      }

      state.capture_plan = *capture_plan_result;
    }
  }

  auto texture =
      Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(frame.Surface());
  if (!texture) {
    Logger().error("Failed to get texture from capture frame");
    return;
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  texture->GetDesc(&source_desc);
  if (source_desc.Width == 0 || source_desc.Height == 0) {
    Logger().error("Failed to resolve recording source texture size");
    return;
  }

  ID3D11Texture2D* current_texture = texture.get();
  auto capture_plan = state.capture_plan;
  if (capture_plan.output_width == 0 || capture_plan.output_height == 0) {
    auto capture_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                         static_cast<int>(source_desc.Width),
                                                         static_cast<int>(source_desc.Height));
    if (!capture_plan_result) {
      Logger().error("Failed to resolve recording crop plan: {}", capture_plan_result.error());
      return;
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
      Logger().error("Failed to refresh recording crop plan: {}", capture_plan_result.error());
      return;
    }
    state.capture_plan = *capture_plan_result;
    capture_plan = *capture_plan_result;
  }

  if (capture_plan.should_crop) {
    // 只录客户区时，先把完整窗口纹理裁成客户区纹理，再交给编码线程。
    auto crop_result = Utils::Graphics::CaptureRegion::crop_texture_to_region(
        state.device.get(), state.context.get(), texture.get(), capture_plan.region,
        state.cropped_texture);
    if (!crop_result) {
      Logger().error("Failed to crop recording frame: {}", crop_result.error());
      return;
    }
    current_texture = *crop_result;
  }

  D3D11_TEXTURE2D_DESC current_desc{};
  current_texture->GetDesc(&current_desc);
  if (current_desc.Width != capture_plan.output_width ||
      current_desc.Height != capture_plan.output_height) {
    Logger().error("Recording frame size mismatch after crop: got {}x{}, expected {}x{}",
                   current_desc.Width, current_desc.Height, capture_plan.output_width,
                   capture_plan.output_height);
    return;
  }

  auto copied_texture_result = copy_texture_for_queue(state, current_texture);
  if (!copied_texture_result) {
    Logger().error("Failed to copy recording frame: {}", copied_texture_result.error());
    return;
  }

  enqueue_video_frame(state, State::QueuedVideoFrame{
                                 .texture = std::move(*copied_texture_result),
                                 .timestamp_100ns = timestamp_100ns,
                             });
}

auto signal_encoder_finish(State::RecordingState& state) -> void {
  {
    std::lock_guard queue_lock(state.queue_mutex);
    // 告诉编码线程：不会再有新数据了，把队列里剩下的写完就收尾。
    state.finish_requested.store(true, std::memory_order_release);
  }
  state.queue_cv.notify_all();
}

auto wait_frame_callback_idle(State::RecordingState& state) -> void {
  // stop_capture 已经阻止后续回调；这里等已经进入 on_frame_arrived 的回调退出。
  std::lock_guard frame_lock(state.frame_mutex);
}

auto cleanup_failed_start(State::RecordingState& state, std::string_view reason) -> void {
  // start 中途失败也走一遍“停输入 -> 等回调 -> 停编码线程 -> 清资源”。
  // 这样失败路径和正常 stop 的资源顺序保持一致。
  state.accepting_input.store(false, std::memory_order_release);
  Utils::Graphics::Capture::stop_capture(state.capture_session);
  wait_frame_callback_idle(state);
  Features::Recording::AudioCapture::stop(state.audio);
  signal_encoder_finish(state);

  if (state.encoder_thread.joinable()) {
    state.encoder_thread.request_stop();
    state.encoder_thread.join();
  }

  Features::Recording::AudioCapture::cleanup(state.audio);
  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);
  delete_working_output_file(state.working_output_path, reason);
  clear_runtime_fields(state);
}

auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  (void)app_state;

  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  clear_runtime_fields(state);

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

  // 录制用独立的 D3D 设备，不复用预览或其他模块的设备，减少互相影响。
  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    clear_runtime_fields(state);
    return std::unexpected("Failed to create D3D device: " + d3d_result.error());
  }
  state.device = d3d_result->first;
  state.context = d3d_result->second;

  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(state.device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    // 捕获回调和编码线程都会用这个 D3D device/context，打开 D3D11 自带的多线程保护。
    multithread->SetMultithreadProtected(TRUE);
  }

  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(state.device.get());
  if (!winrt_device_result) {
    cleanup_failed_start(state, "recording start failed");
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
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

  // WGC 回调只复制纹理并入队，不直接写编码器。
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, state.capture_plan.source_width,
      state.capture_plan.source_height, [&state](auto frame) { on_frame_arrived(state, frame); }, 2,
      capture_options);

  if (!capture_result) {
    cleanup_failed_start(state, "recording start failed");
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  state.start_time = std::chrono::steady_clock::now();
  state.accepting_input.store(true, std::memory_order_release);

  // 从这里开始，WGC 可能随时回调 on_frame_arrived。
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

  // 先停 WGC，再等已经进入的帧回调退出，之后才能清理 D3D/WGC 资源。
  auto stop_capture_start = std::chrono::steady_clock::now();
  Utils::Graphics::Capture::stop_capture(state.capture_session);
  wait_frame_callback_idle(state);
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

  auto dropped_video = state.dropped_video_frames.load(std::memory_order_relaxed);
  auto dropped_audio = state.dropped_audio_packets.load(std::memory_order_relaxed);
  if (dropped_video > 0 || dropped_audio > 0) {
    Logger().warn("Recording queue dropped packets: video={}, audio={}", dropped_video,
                  dropped_audio);
  }

  clear_runtime_fields(state);
  state.status.store(Features::Recording::Types::RecordingStatus::Idle, std::memory_order_release);
  Logger().info("Recording stopped");
}

auto cleanup(Features::Recording::State::RecordingState& state) -> void {
  stop(state);
  MFShutdown();
}

}  // namespace Features::Recording
