module;

#include <winrt/Windows.Graphics.Capture.h>

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Media.AudioCapture;
import Utils.Media.Encoder.State;
import Utils.Timeout;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::State {

// 录制完整状态
export struct RecordingState {
  Features::Recording::Types::RecordingConfig config;
  std::filesystem::path working_output_path;
  HWND target_window = nullptr;
  Features::Recording::Types::CapturePlan capture_plan;

  // 状态标志 - 使用 atomic 避免锁竞争
  std::atomic<Features::Recording::Types::RecordingStatus> status{
      Features::Recording::Types::RecordingStatus::Idle};

  // D3D / WGC / 音频 / 编码器资源
  wil::com_ptr<ID3D11Device> device;
  wil::com_ptr<ID3D11DeviceContext> context;
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device{nullptr};
  bool d3d_initialized = false;
  Utils::Graphics::Capture::CaptureSession capture_session;
  wil::com_ptr<ID3D11Texture2D> cropped_texture;
  Utils::Media::AudioCapture::AudioCaptureContext audio;
  Utils::Media::Encoder::State::EncoderContext encoder;

  // 帧和队列状态
  std::int64_t start_qpc_100ns = 0;
  // 固定 fps 视频时钟与自有编码输入（Copy 自 WGC/crop，无新帧时重复此纹理输出）。
  std::int64_t video_frame_interval_100ns = 10'000'000LL / 30;
  std::int64_t next_video_timestamp_100ns = -1;
  std::int64_t last_emitted_video_timestamp_100ns = -1;
  // stop 一旦开始，就把本段最终要补到的视频时间线冻结下来，避免收尾目标继续跟着墙钟增长。
  std::atomic<std::int64_t> frozen_finish_target_100ns{-1};
  wil::com_ptr<ID3D11Texture2D> encoder_input_texture;
  bool has_encoder_input_texture = false;

  int last_frame_width = 0;
  int last_frame_height = 0;
  std::deque<Features::Recording::Types::QueuedAudioPacket> audio_queue;
  std::atomic<std::uint64_t> dropped_audio_packets{0};
  std::uint64_t discarded_tail_audio_packets = 0;
  std::uint64_t skipped_video_frames_due_to_encoding_lag = 0;
  bool encoder_overload_notified = false;
  std::uint64_t encoded_video_frames = 0;
  std::uint64_t encoded_audio_packets = 0;

  // 编码线程：唯一接触 SinkWriter 的线程
  std::jthread encoder_thread;
  std::atomic<bool> accepting_input{false};
  std::atomic<bool> finish_requested{false};
  std::atomic<bool> has_audio{false};
  bool encoder_ready = false;
  bool encoder_start_succeeded = false;
  bool finalize_succeeded = false;
  std::uint32_t pending_video_frame_count = 0;
  std::string encoder_error;

  // 懒启动的录制控制线程：首次录制请求时启动，之后睡眠等待 toggle / resize / shutdown。
  std::jthread control_thread;
  // 控制线程当前是否正在执行 start/stop/restart；用户 toggle 忙时会被忽略。
  std::atomic<bool> control_action_running{false};
  // 控制请求只用单槽合并；用户 start 请求的参数也挂在这里，避免窗口拖拽时堆积任务。
  Features::Recording::Types::RecordingControlAction pending_action{
      Features::Recording::Types::RecordingControlAction::None};
  std::optional<Features::Recording::Types::StartRequest> pending_start_request;
  // shutdown 开始后置为 true，阻止新的 toggle / resize restart 再抢控制权
  std::atomic<bool> shutdown_requested{false};

  // 线程同步
  // frame_mutex: 保护编码线程主动读取 WGC frame pool 与停止/清理流程。
  std::mutex frame_mutex;
  // queue_mutex: 保护音频队列、待消费视频帧计数和 finish 请求唤醒。
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::mutex encoder_ready_mutex;
  std::condition_variable encoder_ready_cv;
  // control_request_mutex: 保护 pending_action / pending_start_request，不包住真正的 start/stop。
  std::mutex control_request_mutex;
  std::condition_variable control_cv;

  // 延迟释放可复用 D3D 资源，优化高频启停体验。
  std::optional<Utils::Timeout::Timeout> cleanup_timer;
};

}  // namespace Features::Recording::State
