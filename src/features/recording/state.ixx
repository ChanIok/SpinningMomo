module;

#include <mfidl.h>
#include <mfreadwrite.h>

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::Recording::State {

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
};

// 录制完整状态
struct RecordingState {
  Features::Recording::Types::RecordingConfig config;
  Features::Recording::Types::RecordingStatus status =
      Features::Recording::Types::RecordingStatus::Idle;

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

  // 目标窗口信息
  HWND target_window = nullptr;

  // 线程同步
  std::mutex mutex;
};

}  // namespace Features::Recording::State
