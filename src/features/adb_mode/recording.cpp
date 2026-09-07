#include "features/adb_mode/recording.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

#include "features/adb_mode/capture_protocol.hpp"
#include "features/adb_mode/device_session.hpp"
#include "utils/logger/logger.hpp"
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
  const auto parent_dir = session.working_output_path.parent_path();
  if (!parent_dir.empty()) {
    auto ensure_dir = utils::path::EnsureDirectoryExists(parent_dir);
    if (!ensure_dir) {
      return std::unexpected("Failed to ensure directory: " + ensure_dir.error());
    }
  }

  // 1. 创建目标文件的 Byte Stream 与 MPEG-4 Media Sink
  wil::com_ptr<IMFByteStream> byte_stream;
  HRESULT hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE,
                            session.working_output_path.c_str(), byte_stream.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("MFCreateFile failed: hr=0x{:08X}", hr));
  }

  wil::com_ptr<IMFMediaSink> media_sink;
  hr = MFCreateMPEG4MediaSink(byte_stream.get(), nullptr, nullptr, media_sink.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("MFCreateMPEG4MediaSink failed: hr=0x{:08X}", hr));
  }

  // 2. 创建 SinkWriter
  wil::com_ptr<IMFAttributes> attributes;
  hr = MFCreateAttributes(attributes.put(), 2);
  if (FAILED(hr)) {
    return std::unexpected("Failed to create MF attributes");
  }
  attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

  hr = MFCreateSinkWriterFromMediaSink(media_sink.get(), attributes.get(),
                                       session.sink_writer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("MFCreateSinkWriterFromMediaSink failed: hr=0x{:08X}", hr));
  }

  // 3. 视频流直通配置
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

  // 4. 音频流直通配置
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

  // 5. 开启写入
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
    -> void {
  if (!session.sink_writer || !session.has_video_stream || frame.payload.empty()) {
    return;
  }

  auto sample_time_100ns = static_cast<std::int64_t>(frame.timestamp) * 10;
  if (session.last_video_mf_time >= 0 && sample_time_100ns <= session.last_video_mf_time) {
    sample_time_100ns = session.last_video_mf_time + session.video_frame_duration_100ns;
  }

  wil::com_ptr<IMFMediaBuffer> media_buffer;
  HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame.payload.size()), media_buffer.put());
  if (FAILED(hr)) {
    return;
  }

  BYTE* dest = nullptr;
  hr = media_buffer->Lock(&dest, nullptr, nullptr);
  if (FAILED(hr)) {
    return;
  }
  std::memcpy(dest, frame.payload.data(), frame.payload.size());
  media_buffer->Unlock();
  media_buffer->SetCurrentLength(static_cast<DWORD>(frame.payload.size()));

  wil::com_ptr<IMFSample> sample;
  hr = MFCreateSample(sample.put());
  if (FAILED(hr)) {
    return;
  }
  sample->AddBuffer(media_buffer.get());
  sample->SetSampleTime(sample_time_100ns);
  sample->SetSampleDuration(session.video_frame_duration_100ns);

  if ((frame.flags & 1) != 0) {
    sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
  }

  hr = session.sink_writer->WriteSample(session.video_stream_index, sample.get());
  if (FAILED(hr)) {
    Logger().warn("ADB IMFSinkWriter WriteSample (Video) failed: hr=0x{:08X}",
                  static_cast<unsigned long>(hr));
  } else {
    session.last_video_mf_time = sample_time_100ns;
    session.video_samples_written++;
  }
}

// 写入单个音频样本
auto write_audio_sample(AdbRecordingSession& session, const capture_protocol::Frame& frame)
    -> void {
  if (!session.sink_writer || !session.has_audio_stream || frame.payload.empty()) {
    return;
  }

  auto sample_time_100ns = static_cast<std::int64_t>(frame.timestamp) * 10;
  if (session.last_audio_mf_time >= 0 && sample_time_100ns <= session.last_audio_mf_time) {
    sample_time_100ns = session.last_audio_mf_time + session.audio_frame_duration_100ns;
  }

  wil::com_ptr<IMFMediaBuffer> media_buffer;
  HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame.payload.size()), media_buffer.put());
  if (FAILED(hr)) {
    return;
  }

  BYTE* dest = nullptr;
  hr = media_buffer->Lock(&dest, nullptr, nullptr);
  if (FAILED(hr)) {
    return;
  }
  std::memcpy(dest, frame.payload.data(), frame.payload.size());
  media_buffer->Unlock();
  media_buffer->SetCurrentLength(static_cast<DWORD>(frame.payload.size()));

  wil::com_ptr<IMFSample> sample;
  hr = MFCreateSample(sample.put());
  if (FAILED(hr)) {
    return;
  }
  sample->AddBuffer(media_buffer.get());
  sample->SetSampleTime(sample_time_100ns);
  sample->SetSampleDuration(session.audio_frame_duration_100ns);

  hr = session.sink_writer->WriteSample(session.audio_stream_index, sample.get());
  if (FAILED(hr)) {
    Logger().warn("ADB IMFSinkWriter WriteSample (Audio) failed: hr=0x{:08X}",
                  static_cast<unsigned long>(hr));
  } else {
    session.last_audio_mf_time = sample_time_100ns;
    session.audio_samples_written++;
  }
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
  while (true) {
    const auto timeout = session.stop_requested.load(std::memory_order_acquire)
                             ? std::chrono::milliseconds(2000)
                             : std::chrono::milliseconds(5000);

    auto frame_res = session::receive_stream_frame(session.connection, timeout);
    if (!frame_res) {
      if (session.stop_requested.load(std::memory_order_acquire)) {
        Logger().info("ADB recording stream read completed during stop: {}", frame_res.error());
      } else {
        session.error_message = frame_res.error();
        Logger().warn("ADB recording stream read error: {}", frame_res.error());
      }
      break;
    }

    const auto& frame = *frame_res;
    if (frame.type == capture_protocol::MessageType::VideoSample) {
      write_video_sample(session, frame);
    } else if (frame.type == capture_protocol::MessageType::AudioSample) {
      write_audio_sample(session, frame);
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
    if (session.video_samples_written > 0) {
      HRESULT hr = session.sink_writer->Finalize();
      if (SUCCEEDED(hr)) {
        session.finalize_succeeded = true;
        Logger().info(
            "ADB IMFSinkWriter finalized successfully: {} video samples, {} audio samples",
            session.video_samples_written, session.audio_samples_written);
      } else {
        Logger().error("ADB IMFSinkWriter Finalize failed: hr=0x{:08X}",
                       static_cast<unsigned long>(hr));
      }
    } else {
      session.error_message = "No video frames recorded";
      Logger().warn("ADB recording discarded: no video frames written");
    }
  }
}

}  // namespace

auto start(std::shared_ptr<session::DeviceSession> device_session,
           const std::filesystem::path& output_path, std::uint32_t fps, std::uint32_t bitrate,
           bool is_h265) -> std::expected<std::unique_ptr<AdbRecordingSession>, std::string> {
  if (!device_session) {
    return std::unexpected("Device session is null");
  }

  auto conn_result = session::open_recording_connection(*device_session);
  if (!conn_result) {
    return std::unexpected(conn_result.error());
  }

  auto adb_session = std::make_unique<AdbRecordingSession>();
  adb_session->device_session = std::move(device_session);
  adb_session->connection = std::move(conn_result.value());
  adb_session->final_output_path = output_path;
  adb_session->working_output_path = output_path;
  adb_session->working_output_path += L".tmp";
  adb_session->is_h265 = is_h265;

  auto* session_ptr = adb_session.get();
  adb_session->worker_thread = std::jthread([session_ptr, fps, bitrate, is_h265]() {
    worker_thread_proc(*session_ptr, fps, bitrate, is_h265);
  });

  return adb_session;
}

auto stop(std::unique_ptr<AdbRecordingSession>& session) -> AdbRecordResult {
  AdbRecordResult result;
  if (!session) {
    result.kind = AdbRecordResult::Kind::NotRecording;
    return result;
  }

  result.output_path = session->final_output_path;
  session->stop_requested.store(true, std::memory_order_release);

  // 步骤 1：向单条录制连接发送 StopRecord 指令（Android 端会注入 EOS 并回发 RecordFinished）
  static_cast<void>(
      session::send_stream_frame(session->connection, capture_protocol::MessageType::StopRecord));

  // 步骤 2：等待后台消费线程排空落盘退出
  if (session->worker_thread.joinable()) {
    session->worker_thread.join();
  }

  // 步骤 3：关闭连接并移除端口映射
  if (session->device_session) {
    session::close_recording_connection(*session->device_session, session->connection);
  }

  // 步骤 4：根据落盘状态发布产物或清理临时文件
  if (session->finalize_succeeded) {
    std::error_code ec;
    std::filesystem::rename(session->working_output_path, session->final_output_path, ec);
    if (ec) {
      result.kind = AdbRecordResult::Kind::PublishFailed;
      result.error = ec.message();
      Logger().error("Failed to rename temporary ADB recording to final file: {}", ec.message());
    } else {
      result.kind = AdbRecordResult::Kind::Saved;
      if (!session->error_message.empty()) {
        result.error = session->error_message;
        Logger().warn("ADB recording saved with interruption/warning: {}", session->error_message);
      } else {
        Logger().info("ADB recording saved to: {}", session->final_output_path.string());
      }
    }
  } else {
    std::error_code ec;
    std::filesystem::remove(session->working_output_path, ec);
    result.kind = AdbRecordResult::Kind::Discarded;
    result.error =
        session->error_message.empty() ? "ADB recording aborted" : session->error_message;
    Logger().warn("ADB recording discarded: {}", result.error);
  }

  session.reset();
  return result;
}

}  // namespace features::adb_mode::recording
