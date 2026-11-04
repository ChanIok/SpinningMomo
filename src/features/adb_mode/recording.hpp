#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

#include "core/state/app_state.hpp"
#include "features/adb_mode/capture_protocol.hpp"
#include "features/adb_mode/device_session.hpp"

namespace features::adb_mode::recording {

struct AdbRecordResult {
  enum class Kind {
    Saved,
    Discarded,
    Failed,
    NotRecording,
  };

  Kind kind = Kind::NotRecording;
  std::filesystem::path output_path;
  std::string error;
};

// ADB 录制会话实体：统管单通道流 Socket 读取与 IMFSinkWriter MP4 混流落盘
struct AdbRecordingSession {
  std::uint64_t id = 0;
  session::RecordingConnection connection;
  std::filesystem::path output_path;

  wil::com_ptr<IMFSinkWriter> sink_writer;
  DWORD video_stream_index = 0;
  DWORD audio_stream_index = 0;
  bool has_video_stream = false;
  bool has_audio_stream = false;
  bool is_h265 = false;
  bool sink_writer_initialized = false;

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

// 在 AppState 中原地构造本段录制状态，启动失败时清理资源并恢复为空。
// 仅由 ADB 操作队列调用；完成回调在录制线程关闭 writer 后触发，只能投递任务。
auto start(core::AppState& state, const std::filesystem::path& output_path, std::uint32_t fps,
           std::uint32_t bitrate, bool is_h265, std::uint64_t id,
           std::move_only_function<void(std::uint64_t)> on_finished)
    -> std::expected<void, std::string>;

// 由 ADB 操作队列收集结果；会话身份保留到媒体收尾结束，返回后 writer 与连接均已关闭。
auto stop(core::AppState& state) -> AdbRecordResult;

}  // namespace features::adb_mode::recording
