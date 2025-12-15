module;

#include <d3d11.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wil/com.h>
#include <windows.h>

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;

export namespace Features::Recording::State {

// 编码器上下文
struct EncoderContext {
  wil::com_ptr<IMFSinkWriter> sink_writer;
  DWORD video_stream_index = 0;
  wil::com_ptr<ID3D11Texture2D> staging_texture;  // CPU 可读的暂存纹理
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
