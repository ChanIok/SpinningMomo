module;

#include <winrt/Windows.Graphics.Capture.h>

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Media.AudioCapture;
import Utils.Media.Encoder.State;
import Utils.Timeout;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::State {

// 录制几何计划：
// source_* 表示 WGC 实际给到的源帧尺寸；
// output_* 表示最终送给编码器的尺寸；
// should_crop/region 描述是否需要先从源帧裁出一块再编码。
export struct CapturePlan {
  int source_width = 0;
  int source_height = 0;
  std::uint32_t output_width = 0;
  std::uint32_t output_height = 0;
  bool should_crop = false;
  Utils::Graphics::CaptureRegion::CropRegion region{};
};

export inline constexpr std::size_t k_max_audio_queue_size = 120;

export enum class RecordingControlAction {
  None,
  Toggle,
  RestartAfterResize,
  CleanupD3D,
  ShutdownStop,
};

export struct QueuedAudioPacket {
  std::vector<std::uint8_t> data;
  std::uint32_t num_frames = 0;
  std::uint32_t bytes_per_frame = 0;
  std::uint32_t sample_rate = 0;
  std::int64_t timestamp_100ns = 0;
};

// 只描述“视频流时间线”，不持有真实画面。
// WGC 停帧时，音频仍可能继续前进；这时用 SendStreamTick 标记缺失的视频帧，
// 让 SinkWriter 知道视频流不是卡住或被遗漏，而是在这些时间点没有 sample。
export struct VideoTimelineState {
  // 当前录制段的理论视频帧间隔；start() 按配置 fps 预先算好。
  std::int64_t frame_interval_100ns = 10'000'000LL / 30;
  // 半帧容忍窗口，用于判断是否需要给缺失视频帧发送 stream tick。
  std::int64_t half_frame_100ns = (10'000'000LL / 30) / 2;
  // 下一帧视频理论上应该出现的时间；真实视频帧和 stream tick 都会推进它。
  std::int64_t next_expected_timestamp_100ns = 0;
  // 仅用于日志，方便确认最小化/停帧期间是否发过 tick。
  std::uint64_t ticks_sent = 0;
  // 发过 tick 后，下一个真实视频 sample 是 gap 后恢复的第一帧。
  bool sample_after_gap = false;
};

// 录制完整状态
export struct RecordingState {
  Features::Recording::Types::RecordingConfig config;
  std::filesystem::path working_output_path;
  HWND target_window = nullptr;
  CapturePlan capture_plan;

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
  VideoTimelineState video_timeline;
  int last_frame_width = 0;
  int last_frame_height = 0;
  std::deque<QueuedAudioPacket> audio_queue;
  std::atomic<std::uint64_t> dropped_audio_packets{0};
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
  bool video_frame_pending = false;
  std::string encoder_error;

  // 懒启动的录制控制线程：首次录制请求时启动，之后睡眠等待 toggle / resize / shutdown。
  std::jthread control_thread;
  // 控制线程当前是否正在执行 start/stop/restart；用户 toggle 忙时会被忽略。
  std::atomic<bool> control_action_running{false};
  // 控制请求只用单槽合并，避免窗口拖拽时堆积 resize 重启任务。
  RecordingControlAction pending_action{RecordingControlAction::None};
  // shutdown 开始后置为 true，阻止新的 toggle / resize restart 再抢控制权
  std::atomic<bool> shutdown_requested{false};

  // 线程同步
  // frame_mutex: 保护编码线程主动读取 WGC frame pool 与停止/清理流程。
  std::mutex frame_mutex;
  // queue_mutex: 保护音频队列、视频帧通知和 finish 请求唤醒。
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::mutex encoder_ready_mutex;
  std::condition_variable encoder_ready_cv;
  // control_request_mutex: 只保护 pending_action，不包住真正的 start/stop。
  std::mutex control_request_mutex;
  std::condition_variable control_cv;

  // 延迟释放可复用 D3D 资源，优化高频启停体验。
  std::optional<Utils::Timeout::Timeout> cleanup_timer;
};

}  // namespace Features::Recording::State
