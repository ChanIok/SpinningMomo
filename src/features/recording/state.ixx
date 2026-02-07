module;

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Media.Encoder.State;
import <audioclient.h>;
import <d3d11.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::Recording::State {

// 音频捕获上下文
struct AudioCaptureContext {
  wil::com_ptr<IMMDevice> device;                    // 音频设备
  wil::com_ptr<IAudioClient> audio_client;           // 音频客户端
  wil::com_ptr<IAudioCaptureClient> capture_client;  // 捕获客户端

  WAVEFORMATEX* wave_format = nullptr;  // 音频格式
  UINT32 buffer_frame_count = 0;        // 缓冲区帧数

  HANDLE audio_event = nullptr;           // WASAPI 缓冲就绪事件
  std::jthread capture_thread;            // 捕获线程
  std::atomic<bool> should_stop = false;  // 停止信号
};

// 录制完整状态
struct RecordingState {
  Features::Recording::Types::RecordingConfig config;

  // 状态标志 - 使用 atomic 避免锁竞争
  std::atomic<Features::Recording::Types::RecordingStatus> status{
      Features::Recording::Types::RecordingStatus::Idle};

  // D3D 资源 (Headless)
  wil::com_ptr<ID3D11Device> device;
  wil::com_ptr<ID3D11DeviceContext> context;

  // WGC 捕获会话
  Utils::Graphics::Capture::CaptureSession capture_session;

  // 编码器（使用共享编码器模块）
  Utils::Media::Encoder::State::EncoderContext encoder;

  // 帧率控制
  std::chrono::steady_clock::time_point start_time;
  uint64_t frame_index = 0;

  // 最后编码的帧纹理（用于帧重复填充）
  wil::com_ptr<ID3D11Texture2D> last_encoded_texture;

  // 目标窗口信息
  HWND target_window = nullptr;

  // 音频捕获
  AudioCaptureContext audio;

  // 线程同步
  // encoder_write_mutex: 保护 sink_writer 的写入操作（视频帧回调和音频线程共享）
  std::mutex encoder_write_mutex;
  // resource_mutex: 保护资源的初始化、清理和帧填充逻辑（主线程独占）
  std::mutex resource_mutex;
};

}  // namespace Features::Recording::State
