#pragma once

#include "vendor/std.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfidl.hpp"

#include "features/recording/types.hpp"
#include "utils/media/mft.hpp"

namespace features::recording {

// 单路编码输出流缓冲区与状态
struct EncodedStream {
  wil::com_ptr<IMFMediaType> type;
  std::deque<utils::media::mft::Packet> packets;
  bool ready = false;
  bool done = false;
};

// 单段录制会话：持有音视频工作线程、互斥锁、帧池与时间锚点
// 单段资源存活到所有工作线程退出，回调解除后才能销毁会话
struct RecordingSession {
  RecordingConfig config;
  HWND target_window = nullptr;
  bool prefer_high_performance_gpu = true;

  std::mutex mutex;
  std::condition_variable changed;

  // 0=video，1=audio
  std::array<EncodedStream, 2> streams;

  // 压缩数据包总内存追踪与峰值
  std::size_t packet_bytes = 0;
  std::size_t peak_packet_bytes = 0;
  // 告警水位起始 QPC，低于水位归零；由 mutex 保护。
  std::int64_t packet_warning_since_100ns = 0;

  // 连续 PCM 采样缓冲与时钟基准
  std::deque<QueuedAudioPacket> pcm;
  std::size_t pcm_frames = 0;
  std::int64_t audio_anchor_qpc_100ns = 0;
  std::uint64_t audio_source_frames = 0;
  bool audio_available = false;

  // 定时精度与原子生命周期标志
  bool timer_period_requested = false;
  std::atomic<bool> started{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> output_created{false};

  // 绝对时间基准戳（100ns）
  std::int64_t start_qpc_100ns = 0;
  std::atomic<std::int64_t> stop_qpc_100ns{-1};
  std::string error;

  // 视频统计与监控指标
  std::atomic<std::uint64_t> video_captured{0};
  std::atomic<std::uint64_t> video_submitted{0};
  std::atomic<std::uint64_t> video_encoded{0};
  std::atomic<std::uint64_t> video_skipped{0};
  std::atomic<std::int64_t> video_delay_100ns{0};

  // 音频统计指标
  std::atomic<std::uint64_t> audio_encoded{0};
  std::atomic<std::uint64_t> audio_silent_frames{0};
  std::atomic<std::uint64_t> audio_discarded_frames{0};

  // 纹理池占用峰值与流水线运行阶段
  std::atomic<UINT> pool_peak{0};
  std::atomic<const char*> video_stage{"prepare"};
  std::atomic<const char*> audio_stage{"prepare"};
  std::atomic<const char*> mux_stage{"prepare"};

  // 三路核心工作线程
  std::jthread video_thread;
  std::jthread audio_thread;
  std::jthread mux_thread;
};

// 全局录制模块状态（挂载于 AppState）
struct RecordingState {
  std::atomic<RecordingStatus> status{RecordingStatus::Idle};

  // 空闲时为空，每段原地构造；发布/销毁使用 control_request_mutex，数据面使用 session.mutex
  std::optional<RecordingSession> session;

  std::atomic<bool> control_action_running{false};
  std::atomic<bool> shutdown_requested{false};
  RecordingControlAction pending_action{RecordingControlAction::None};
  std::optional<StartRequest> pending_start_request;

  std::mutex control_request_mutex;
  std::condition_variable control_cv;

  // 后台控制线程：最先析构，停止唤醒所依赖的锁/CV仍存活
  std::jthread control_thread;
};

}  // namespace features::recording
