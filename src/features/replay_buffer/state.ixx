module;

export module Features.ReplayBuffer.State;

import std;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.DiskRingBuffer;
import Utils.Graphics.Capture;
import Utils.Media.AudioCapture;
import Utils.Media.RawEncoder;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::ReplayBuffer::State {

// 回放缓冲完整状态（新架构：单编码器 + 硬盘环形缓冲）
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

  // RawEncoder（单一实例，全程运行）
  Utils::Media::RawEncoder::RawEncoderContext raw_encoder;

  // 硬盘环形缓冲
  DiskRingBuffer::DiskRingBufferContext ring_buffer;

  // 帧率控制
  std::chrono::steady_clock::time_point start_time;
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

  // 缓存目录
  std::filesystem::path cache_dir;
};

}  // namespace Features::ReplayBuffer::State
