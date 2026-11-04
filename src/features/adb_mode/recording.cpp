#include "features/adb_mode/recording.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

#include "core/state/app_state.hpp"
#include "features/adb_mode/capture_protocol.hpp"
#include "features/adb_mode/device_session.hpp"
#include "features/adb_mode/state.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/hybrid_mp4.hpp"
#include "utils/media/sink_writer.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace features::adb_mode::recording {

namespace {

// 从大端序缓冲区解析 32 位无符号整数
auto read_be_u32(const std::uint8_t* data) -> std::uint32_t {
  return (static_cast<std::uint32_t>(data[0]) << 24u) |
         (static_cast<std::uint32_t>(data[1]) << 16u) |
         (static_cast<std::uint32_t>(data[2]) << 8u) | static_cast<std::uint32_t>(data[3]);
}

// 从大端序缓冲区解析 64 位无符号整数
auto read_be_u64(const std::uint8_t* data) -> std::uint64_t {
  return (static_cast<std::uint64_t>(read_be_u32(data)) << 32u) |
         static_cast<std::uint64_t>(read_be_u32(data + 4));
}

// 写入 32 位无符号整数（大端序）
auto write_be_u32(std::uint8_t* dest, std::uint32_t value) -> void {
  dest[0] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
  dest[1] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
  dest[2] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
  dest[3] = static_cast<std::uint8_t>(value & 0xFFu);
}

struct RecordReadyInfo {
  std::uint64_t time_origin_us = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t fps = 0;
  bool is_h265 = false;
  std::vector<std::uint8_t> video_config;
  bool has_audio = false;
  std::uint32_t audio_sample_rate = 0;
  std::uint32_t audio_channels = 0;
  std::vector<std::uint8_t> audio_config;
};

// 解析 Android 端发送的 RECORD_READY 握手帧载荷
auto parse_record_ready(std::span<const std::uint8_t> payload)
    -> std::expected<RecordReadyInfo, std::string> {
  if (payload.size() < 25) {
    return std::unexpected("RecordReady payload too small");
  }
  RecordReadyInfo info;
  std::size_t offset = 0;
  info.time_origin_us = read_be_u64(payload.data() + offset);
  offset += 8;
  info.width = read_be_u32(payload.data() + offset);
  offset += 4;
  info.height = read_be_u32(payload.data() + offset);
  offset += 4;
  info.fps = read_be_u32(payload.data() + offset);
  offset += 4;
  info.is_h265 = (payload[offset] != 0);
  offset += 1;

  const auto video_config_len = read_be_u32(payload.data() + offset);
  offset += 4;
  if (offset + video_config_len > payload.size()) {
    return std::unexpected("Invalid video config length in RecordReady");
  }
  info.video_config.assign(payload.data() + offset, payload.data() + offset + video_config_len);
  offset += video_config_len;

  if (offset < payload.size()) {
    info.has_audio = (payload[offset] != 0);
    offset += 1;
    if (info.has_audio) {
      if (offset + 12 > payload.size()) {
        return std::unexpected("Truncated audio header in RecordReady");
      }
      info.audio_sample_rate = read_be_u32(payload.data() + offset);
      offset += 4;
      info.audio_channels = read_be_u32(payload.data() + offset);
      offset += 4;
      const auto audio_config_len = read_be_u32(payload.data() + offset);
      offset += 4;
      if (offset + audio_config_len > payload.size()) {
        return std::unexpected("Invalid audio config length in RecordReady");
      }
      info.audio_config.assign(payload.data() + offset, payload.data() + offset + audio_config_len);
    }
  }
  return info;
}

// 初始化 Media Foundation SinkWriter 为零转码 Passthrough 模式
auto initialize_sink_writer(AdbRecordingSession& session, const RecordReadyInfo& info,
                            std::uint32_t target_bitrate) -> std::expected<void, std::string> {
  // 1. 准备输出目录与 Writer 属性，后续按 Android 已编码的格式直通写入。
  const auto parent_dir = session.output_path.parent_path();
  if (!parent_dir.empty()) {
    auto ensure_dir = utils::path::EnsureDirectoryExists(parent_dir);
    if (!ensure_dir) {
      return std::unexpected("Failed to ensure directory: " + ensure_dir.error());
    }
  }

  // 禁用 Writer 自身节流，避免录制通道消费被额外限速。
  wil::com_ptr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(attributes.put(), 3);
  if (FAILED(hr)) {
    return std::unexpected("Failed to create MF attributes");
  }
  attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

  // 2. 创建分片 MP4 Writer，录制过程中持续落下可保留的完整分片。
  auto writer_result = utils::media::sink_writer::create_fragmented(
      session.output_path, attributes.get(), session.sink_writer);
  if (!writer_result) return std::unexpected(writer_result.error());

  // 3. 按握手给出的视频参数配置直通流，不重新编码视频。
  wil::com_ptr<IMFMediaType> video_type;
  hr = MFCreateMediaType(video_type.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create video media type");
  }
  video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  video_type->SetGUID(MF_MT_SUBTYPE, info.is_h265 ? MFVideoFormat_HEVC : MFVideoFormat_H264);
  video_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  MFSetAttributeSize(video_type.get(), MF_MT_FRAME_SIZE, info.width, info.height);
  MFSetAttributeRatio(video_type.get(), MF_MT_FRAME_RATE, info.fps > 0 ? info.fps : 60, 1);
  MFSetAttributeRatio(video_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  video_type->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate > 0 ? target_bitrate : 16'000'000);

  if (!info.video_config.empty()) {
    video_type->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, info.video_config.data(),
                        static_cast<UINT32>(info.video_config.size()));
  }

  hr = session.sink_writer->AddStream(video_type.get(), &session.video_stream_index);
  if (FAILED(hr)) {
    return std::unexpected(std::format("AddStream (Video) failed: hr=0x{:08X}", hr));
  }
  hr =
      session.sink_writer->SetInputMediaType(session.video_stream_index, video_type.get(), nullptr);
  if (FAILED(hr)) {
    return std::unexpected(std::format("SetInputMediaType (Video) failed: hr=0x{:08X}", hr));
  }
  session.has_video_stream = true;
  session.is_h265 = info.is_h265;
  session.video_frame_duration_100ns = 10'000'000LL / (info.fps > 0 ? info.fps : 60);

  // 4. 配置可选 AAC 音频直通流，保持与设备端编码格式一致。
  if (info.has_audio) {
    wil::com_ptr<IMFMediaType> audio_type;
    hr = MFCreateMediaType(audio_type.put());
    if (FAILED(hr)) {
      return std::unexpected("Failed to create audio media type");
    }
    audio_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    audio_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                          info.audio_sample_rate > 0 ? info.audio_sample_rate : 48000);
    audio_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,
                          info.audio_channels > 0 ? info.audio_channels : 2);
    audio_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    audio_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 128000 / 8);
    audio_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    audio_type->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);

    hr = session.sink_writer->AddStream(audio_type.get(), &session.audio_stream_index);
    if (FAILED(hr)) {
      Logger().warn("AddStream (Audio) failed: hr=0x{:08X}, continuing video-only", hr);
    } else {
      hr = session.sink_writer->SetInputMediaType(session.audio_stream_index, audio_type.get(),
                                                  nullptr);
      if (FAILED(hr)) {
        Logger().warn("SetInputMediaType (Audio) failed: hr=0x{:08X}, continuing video-only", hr);
      } else {
        session.has_audio_stream = true;
        const auto sample_rate = info.audio_sample_rate > 0 ? info.audio_sample_rate : 48000;
        session.audio_frame_duration_100ns = 1024LL * 10'000'000LL / sample_rate;
      }
    }
  }

  // 5. 所有流配置完成后开始写入，工作线程随后才能提交样本。
  hr = session.sink_writer->BeginWriting();
  if (FAILED(hr)) {
    return std::unexpected(std::format("BeginWriting failed: hr=0x{:08X}", hr));
  }

  session.sink_writer_initialized = true;
  Logger().info("ADB IMFSinkWriter initialized: video={}x{}@{}fps (config={} bytes), audio={}{}Hz",
                info.width, info.height, info.fps, info.video_config.size(),
                session.has_audio_stream ? "enabled " : "disabled",
                info.audio_sample_rate > 0 ? info.audio_sample_rate : 48000);
  return {};
}

// 写入单个视频样本
auto write_video_sample(AdbRecordingSession& session, const capture_protocol::Frame& frame)
    -> std::expected<void, std::string> {
  if (!session.sink_writer || !session.has_video_stream || frame.payload.empty()) {
    return {};
  }

  auto sample_time_100ns = static_cast<std::int64_t>(frame.timestamp) * 10;
  if (session.last_video_mf_time >= 0 && sample_time_100ns <= session.last_video_mf_time) {
    sample_time_100ns = session.last_video_mf_time + session.video_frame_duration_100ns;
  }

  wil::com_ptr<IMFMediaBuffer> media_buffer;
  HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame.payload.size()), media_buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }

  BYTE* dest = nullptr;
  hr = media_buffer->Lock(&dest, nullptr, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  std::memcpy(dest, frame.payload.data(), frame.payload.size());
  media_buffer->Unlock();
  media_buffer->SetCurrentLength(static_cast<DWORD>(frame.payload.size()));

  wil::com_ptr<IMFSample> sample;
  hr = MFCreateSample(sample.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  sample->AddBuffer(media_buffer.get());
  sample->SetSampleTime(sample_time_100ns);
  sample->SetSampleDuration(session.video_frame_duration_100ns);

  if ((frame.flags & 1) != 0) {
    sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
  }

  hr = session.sink_writer->WriteSample(session.video_stream_index, sample.get());
  if (FAILED(hr)) {
    return std::unexpected(std::format("ADB IMFSinkWriter WriteSample (Video) failed: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  session.last_video_mf_time = sample_time_100ns;
  session.video_samples_written++;
  return {};
}

// 写入单个音频样本
auto write_audio_sample(AdbRecordingSession& session, const capture_protocol::Frame& frame)
    -> std::expected<void, std::string> {
  if (!session.sink_writer || !session.has_audio_stream || frame.payload.empty()) {
    return {};
  }

  auto sample_time_100ns = static_cast<std::int64_t>(frame.timestamp) * 10;
  if (session.last_audio_mf_time >= 0 && sample_time_100ns <= session.last_audio_mf_time) {
    sample_time_100ns = session.last_audio_mf_time + session.audio_frame_duration_100ns;
  }

  wil::com_ptr<IMFMediaBuffer> media_buffer;
  HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame.payload.size()), media_buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }

  BYTE* dest = nullptr;
  hr = media_buffer->Lock(&dest, nullptr, nullptr);
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  std::memcpy(dest, frame.payload.data(), frame.payload.size());
  media_buffer->Unlock();
  media_buffer->SetCurrentLength(static_cast<DWORD>(frame.payload.size()));

  wil::com_ptr<IMFSample> sample;
  hr = MFCreateSample(sample.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to prepare ADB recording sample: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  sample->AddBuffer(media_buffer.get());
  sample->SetSampleTime(sample_time_100ns);
  sample->SetSampleDuration(session.audio_frame_duration_100ns);

  hr = session.sink_writer->WriteSample(session.audio_stream_index, sample.get());
  if (FAILED(hr)) {
    return std::unexpected(std::format("ADB IMFSinkWriter WriteSample (Audio) failed: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }
  session.last_audio_mf_time = sample_time_100ns;
  session.audio_samples_written++;
  return {};
}

// 录制流后台消费线程：发送 StartRecord → 等待 RecordReady → 接收 Sample 直至 Finished
auto worker_thread_proc(AdbRecordingSession& session, std::uint32_t fps, std::uint32_t bitrate,
                        bool is_h265) -> void {
  const auto hr_com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const auto com_guard = wil::scope_exit([hr_com]() {
    if (SUCCEEDED(hr_com)) {
      CoUninitialize();
    }
  });

  const auto hr_mf = MFStartup(MF_VERSION);
  const auto mf_guard = wil::scope_exit([hr_mf]() {
    if (SUCCEEDED(hr_mf)) {
      MFShutdown();
    }
  });
  const auto writer_guard = wil::scope_exit([&session]() {
    auto closed = utils::media::sink_writer::close(session.sink_writer, false);
    if (!closed) Logger().warn("ADB writer cleanup failed: {}", closed.error());
  });
  if (FAILED(hr_com) || FAILED(hr_mf)) {
    session.error_message = "Failed to initialize ADB recording media runtime";
    return;
  }

  // 1. 向 Android 录制通道发送 START_RECORD 请求帧
  std::array<std::uint8_t, 9> start_payload{};
  write_be_u32(start_payload.data(), fps);
  write_be_u32(start_payload.data() + 4, bitrate);
  start_payload[8] = is_h265 ? 1 : 0;

  auto send_res = session::send_stream_frame(
      session.connection, capture_protocol::MessageType::StartRecord, start_payload);
  if (!send_res) {
    session.error_message = "Failed to send StartRecord: " + send_res.error();
    Logger().error("ADB recording worker: {}", session.error_message);
    return;
  }

  // 2. 接收并解析首个确认握手帧 RECORD_READY
  auto ready_frame_res =
      session::receive_stream_frame(session.connection, std::chrono::milliseconds(5000));
  if (!ready_frame_res) {
    session.error_message = "Failed to receive RecordReady: " + ready_frame_res.error();
    Logger().error("ADB recording worker: {}", session.error_message);
    return;
  }

  if (ready_frame_res->type == capture_protocol::MessageType::Error) {
    session.error_message = session::capture_error(*ready_frame_res);
    Logger().error("ADB recording worker received error on start: {}", session.error_message);
    return;
  }

  if (ready_frame_res->type != capture_protocol::MessageType::RecordReady) {
    session.error_message = "Unexpected message type before stream: " +
                            std::to_string(static_cast<int>(ready_frame_res->type));
    Logger().error("ADB recording worker: {}", session.error_message);
    return;
  }

  auto ready_info = parse_record_ready(ready_frame_res->payload);
  if (!ready_info) {
    session.error_message = "Failed to parse RecordReady: " + ready_info.error();
    Logger().error("ADB recording worker: {}", session.error_message);
    return;
  }

  // 3. 收到明确流格式后，立即完成 IMFSinkWriter 的零转码初始化
  auto init_res = initialize_sink_writer(session, *ready_info, bitrate);
  if (!init_res) {
    session.error_message = "Failed to initialize IMFSinkWriter: " + init_res.error();
    Logger().error("ADB recording worker: {}", session.error_message);
    return;
  }

  // 4. 循环读取并消费音视频样本
  bool stop_sent = false;
  while (true) {
    // 录制连接的局部 Asio 循环只归此线程；控制线程不能并发 restart/run 来发送停止。
    if (!stop_sent && session.stop_requested.load(std::memory_order_acquire)) {
      auto stopped =
          session::send_stream_frame(session.connection, capture_protocol::MessageType::StopRecord);
      if (!stopped) {
        session.error_message = stopped.error();
        break;
      }
      stop_sent = true;
    }
    const auto timeout =
        stop_sent ? std::chrono::milliseconds(2000) : std::chrono::milliseconds(5000);

    auto frame_res = session::receive_stream_frame(session.connection, timeout);
    if (!frame_res) {
      // 只有 RecordFinished 才代表完整结束；停止期间断流也不能伪装成正常录像。
      session.error_message = frame_res.error();
      Logger().warn("ADB recording stream read error: {}", frame_res.error());
      break;
    }

    const auto& frame = *frame_res;
    if (frame.type == capture_protocol::MessageType::VideoSample ||
        frame.type == capture_protocol::MessageType::AudioSample) {
      auto written = frame.type == capture_protocol::MessageType::VideoSample
                         ? write_video_sample(session, frame)
                         : write_audio_sample(session, frame);
      if (!written) {
        session.error_message = written.error();
        break;
      }
    } else if (frame.type == capture_protocol::MessageType::AudioEnded) {
      Logger().info("ADB recording received AudioEnded; continuing video-only");
      session.has_audio_stream = false;
    } else if (frame.type == capture_protocol::MessageType::RecordFinished) {
      Logger().info("ADB recording stream received RecordFinished");
      break;
    } else if (frame.type == capture_protocol::MessageType::Error) {
      session.error_message = session::capture_error(frame);
      Logger().error("ADB recording stream error: {}", session.error_message);
      break;
    }
  }

  // 5. 排空完成后执行 SinkWriter 落盘
  if (session.sink_writer && session.sink_writer_initialized) {
    if (session.video_samples_written > 0 && session.error_message.empty()) {
      auto closed = utils::media::sink_writer::close(session.sink_writer, true);
      if (closed) {
        Logger().info(
            "ADB IMFSinkWriter finalized successfully: {} video samples, {} audio samples",
            session.video_samples_written, session.audio_samples_written);
      } else {
        session.error_message = closed.error();
        Logger().error("ADB IMFSinkWriter close failed: {}", closed.error());
      }
    } else if (session.video_samples_written == 0 && session.error_message.empty()) {
      session.error_message = "No video frames recorded";
      Logger().warn("ADB recording discarded: no video frames written");
    }
  }
}

}  // namespace

auto start(core::AppState& state, const std::filesystem::path& output_path, std::uint32_t fps,
           std::uint32_t bitrate, bool is_h265, std::uint64_t id,
           std::move_only_function<void(std::uint64_t)> on_finished)
    -> std::expected<void, std::string> {
  // 1. 校验设备连接并原地构造本段状态，段 ID 用于识别异步完成回报。
  auto device_session = session::get_active(state);
  if (!device_session) return std::unexpected(device_session.error());

  auto& adb_state = *state.adb_mode;
  AdbRecordingSession* recording;
  {
    std::scoped_lock lock(adb_state.mutex);
    if (adb_state.recording_session)
      return std::unexpected("ADB recording session is already active");
    recording = &adb_state.recording_session.emplace();
    recording->id = id;
  }
  // 线程启动是最后一步；此前失败时尚无运行线程，可以直接清理并销毁本段。
  auto rollback = wil::scope_exit([&] {
    session::close_recording_connection(**device_session, recording->connection);
    std::scoped_lock lock(adb_state.mutex);
    adb_state.recording_session.reset();
  });

  // 2. 拒绝覆盖已有录像，再保存本段输出参数。
  std::error_code path_error;
  if (std::filesystem::exists(output_path, path_error))
    return std::unexpected("Recording output already exists: " + output_path.string());
  if (path_error)
    return std::unexpected("Failed to inspect recording output: " + path_error.message());

  recording->output_path = output_path;
  recording->is_h265 = is_h265;
  // 3. 建立独立录制通道，避免音视频流占用设备控制通道。
  auto connection = session::open_recording_connection(**device_session);
  if (!connection) return std::unexpected(connection.error());
  recording->connection = std::move(*connection);

  // 4. 最后启动消费线程；握手、媒体初始化和样本写入都由该线程执行。
  try {
    recording->worker_thread = std::jthread(
        [recording, fps, bitrate, is_h265, id, on_finished = std::move(on_finished)]() mutable {
          try {
            worker_thread_proc(*recording, fps, bitrate, is_h265);
          } catch (const std::exception& e) {
            recording->error_message = e.what();
          } catch (...) {
            recording->error_message = "Unknown ADB recording worker error";
          }
          on_finished(id);
        });
  } catch (const std::system_error& e) {
    return std::unexpected("Failed to start ADB recording worker: " + std::string(e.what()));
  }

  rollback.release();
  return {};
}

auto stop(core::AppState& state) -> AdbRecordResult {
  auto& adb_state = *state.adb_mode;
  AdbRecordingSession* session;
  {
    std::scoped_lock lock(adb_state.mutex);
    if (!adb_state.recording_session) return {};
    session = &*adb_state.recording_session;
  }
  // 1. 提交停止意图；StopRecord 仍由消费线程发送，避免并发操作录制连接。
  session->stop_requested.store(true, std::memory_order_release);

  // 2. 在状态锁外等待消费线程排空并关闭 Writer；自然结束时只需收集结果。
  if (session->worker_thread.joinable()) {
    session->worker_thread.join();
  }

  // 3. Writer 已关闭，回收录制连接及其端口映射。
  session::close_recording_connection(adb_state.device_session, session->connection);

  // join 在状态锁外完成；索引整理期间仍保留本段身份，之后才销毁可复用存储中的对象。
  auto clear_session = wil::scope_exit([&] {
    std::scoped_lock lock(adb_state.mutex);
    adb_state.recording_session.reset();
  });
  AdbRecordResult result;
  result.output_path = session->output_path;

  // 4. 有效且正常结束时整理总索引；异常保留已有分片，没有视频样本才丢弃。
  if (session->video_samples_written > 0) {
    result.error = session->error_message;
    if (result.error.empty()) {
      auto finalized = utils::media::hybrid_mp4::finalize(session->output_path);
      if (!finalized) result.error = finalized.error();
    }
    result.kind =
        result.error.empty() ? AdbRecordResult::Kind::Saved : AdbRecordResult::Kind::Failed;
    if (!result.error.empty())
      Logger().warn("ADB recording retained at '{}': {}", result.output_path.string(),
                    result.error);
  } else {
    std::error_code ec;
    std::filesystem::remove(session->output_path, ec);
    result.kind = AdbRecordResult::Kind::Discarded;
    result.error =
        session->error_message.empty() ? "ADB recording aborted" : session->error_message;
    Logger().warn("ADB recording discarded: {}", result.error);
  }

  return result;
}

}  // namespace features::adb_mode::recording
