module;

#include <mfidl.h>
#include <mfreadwrite.h>

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
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

// 编码器上下文
struct EncoderContext {
  wil::com_ptr<IMFSinkWriter> sink_writer;
  DWORD video_stream_index = 0;

  // 缓存的尺寸信息
  uint32_t frame_width = 0;
  uint32_t frame_height = 0;
  DWORD buffer_size = 0;  // width * height * 4

  // CPU 编码模式
  wil::com_ptr<ID3D11Texture2D> staging_texture;  // CPU 可读的暂存纹理
  wil::com_ptr<IMFSample> reusable_sample;        // 复用的 Sample
  wil::com_ptr<IMFMediaBuffer> reusable_buffer;   // 复用的 Buffer

  // GPU 编码模式
  wil::com_ptr<IMFDXGIDeviceManager> dxgi_manager;
  UINT reset_token = 0;
  wil::com_ptr<ID3D11Texture2D> shared_texture;  // 编码器专用纹理
  bool gpu_encoding = false;

  // 音频流
  DWORD audio_stream_index = 0;  // 音频流索引
  bool has_audio = false;        // 是否有音频流
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

  // 编码器
  EncoderContext encoder;

  // 帧率控制
  std::chrono::steady_clock::time_point start_time;
  uint64_t frame_index = 0;

  // 最后编码的帧纹理（用于帧重复填充）
  wil::com_ptr<ID3D11Texture2D> last_encoded_texture;

  // 目标窗口信息
  HWND target_window = nullptr;

  // 音频捕获
  AudioCaptureContext audio;

  // 线程同步 - 细粒度锁
  // encoder_write_mutex: 保护 sink_writer 的写入操作（视频帧回调和音频线程共享）
  std::mutex encoder_write_mutex;
  // resource_mutex: 保护资源的初始化、清理和帧填充逻辑（主线程独占）
  std::mutex resource_mutex;
};

}  // namespace Features::Recording::State
