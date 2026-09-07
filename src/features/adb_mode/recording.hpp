#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

#include "features/adb_mode/capture_protocol.hpp"
#include "features/adb_mode/device_session.hpp"

namespace features::adb_mode::recording {

struct AdbRecordResult {
  enum class Kind {
    Saved,
    Discarded,
    PublishFailed,
    NotRecording,
  };

  Kind kind = Kind::NotRecording;
  std::filesystem::path output_path;
  std::string error;
};

// ADB 录制会话实体：统管单通道流 Socket 读取与 IMFSinkWriter MP4 混流落盘
struct AdbRecordingSession {
  std::shared_ptr<session::DeviceSession> device_session;
  session::RecordingConnection connection;
  std::filesystem::path final_output_path;
  std::filesystem::path working_output_path;

  wil::com_ptr<IMFSinkWriter> sink_writer;
  DWORD video_stream_index = 0;
  DWORD audio_stream_index = 0;
  bool has_video_stream = false;
  bool has_audio_stream = false;
  bool is_h265 = false;
  bool sink_writer_initialized = false;
  bool finalize_succeeded = false;

  std::uint64_t video_samples_written = 0;
  std::uint64_t audio_samples_written = 0;
  std::int64_t last_video_mf_time = -1;
  std::int64_t last_audio_mf_time = -1;
  std::int64_t video_frame_duration_100ns = 166'666;
  std::int64_t audio_frame_duration_100ns = 213'333;

  std::jthread worker_thread;
  std::atomic<bool> stop_requested{false};
  std::string error_message;
};

// 启动 ADB 录制会话：连接流 socket → 发送 StartRecord → 接收 RecordReady 初始化 SinkWriter →
// 开启流式消费
auto start(std::shared_ptr<session::DeviceSession> device_session,
           const std::filesystem::path& output_path, std::uint32_t fps, std::uint32_t bitrate,
           bool is_h265 = false)
    -> std::expected<std::unique_ptr<AdbRecordingSession>, std::string>;

// 停止 ADB 录制会话：发送 StopRecord 指令 → Finalize IMFSinkWriter → 发布 mp4 文件
auto stop(std::unique_ptr<AdbRecordingSession>& session) -> AdbRecordResult;

}  // namespace features::adb_mode::recording
