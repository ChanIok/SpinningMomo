module;

export module Features.ReplayBuffer.State;

import std;
import Features.ReplayBuffer.Types;
import Utils.Graphics.Capture;
import Utils.Media.AudioCapture;
import Utils.Media.Encoder.State;
import Utils.Timer;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::ReplayBuffer::State {

// 已完成的视频段落信息
struct SegmentInfo {
  std::filesystem::path path;                        // 段落文件路径
  std::chrono::steady_clock::time_point start_time;  // 段落开始时间
  std::int64_t duration_100ns = 0;                   // 段落实际时长（100ns 单位）
};

// 回放缓冲完整状态
struct ReplayBufferState {
  Features::ReplayBuffer::Types::ReplayBufferConfig config;

  // 状态标志 - 使用 atomic 避免锁竞争
  std::atomic<Features::ReplayBuffer::Types::ReplayBufferStatus> status{
      Features::ReplayBuffer::Types::ReplayBufferStatus::Idle};

  // 即时回放运行时开关（不持久化）
  // Motion Photo 的 enabled 从 settings 读取
  std::atomic<bool> replay_enabled{false};

  // D3D 资源 (Headless)
  wil::com_ptr<ID3D11Device> device;
  wil::com_ptr<ID3D11DeviceContext> context;

  // WGC 捕获会话
  Utils::Graphics::Capture::CaptureSession capture_session;

  // 编码器（使用共享编码器模块）
  Utils::Media::Encoder::State::EncoderContext encoder;

  // 段落管理
  SegmentInfo current_segment;
  std::deque<SegmentInfo> completed_segments;

  // 帧率控制
  std::chrono::steady_clock::time_point segment_start_time;
  std::uint64_t frame_index = 0;

  // 最后编码的帧纹理（用于帧重复填充）
  wil::com_ptr<ID3D11Texture2D> last_encoded_texture;

  // 音频捕获（使用共享音频捕获模块）
  Utils::Media::AudioCapture::AudioCaptureContext audio;

  // 目标窗口
  HWND target_window = nullptr;

  // 线程同步
  std::mutex encoder_write_mutex;
  std::mutex resource_mutex;

  // 临时文件目录
  std::filesystem::path temp_dir;

  // 轮转定时器
  std::optional<Utils::Timer::Timer> rotation_timer;
};

}  // namespace Features::ReplayBuffer::State
