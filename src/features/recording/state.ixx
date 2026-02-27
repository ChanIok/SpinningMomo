module;

export module Features.Recording.State;

import std;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Media.AudioCapture;
import Utils.Media.Encoder.State;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::Recording::State {

// 录制完整状态
struct RecordingState {
  Features::Recording::Types::RecordingConfig config;

  // 状态标志 - 使用 atomic 避免锁竞争
  std::atomic<Features::Recording::Types::RecordingStatus> status{
      Features::Recording::Types::RecordingStatus::Idle};
  // 防止重复触发录制切换任务
  std::atomic<bool> op_in_progress{false};
  // 录制开关专用线程（仅执行开始/停止控制逻辑）
  std::jthread toggle_thread;

  // D3D 资源 (Headless)
  wil::com_ptr<ID3D11Device> device;
  wil::com_ptr<ID3D11DeviceContext> context;

  // WGC 捕获会话
  Utils::Graphics::Capture::CaptureSession capture_session;
  bool crop_to_client_area = false;
  Utils::Graphics::CaptureRegion::CropRegion crop_region;
  wil::com_ptr<ID3D11Texture2D> cropped_texture;

  // 编码器（使用共享编码器模块）
  Utils::Media::Encoder::State::EncoderContext encoder;

  // 帧率控制
  std::chrono::steady_clock::time_point start_time;
  std::uint64_t frame_index = 0;

  // 最后编码的帧纹理（用于帧重复填充）
  wil::com_ptr<ID3D11Texture2D> last_encoded_texture;

  // 目标窗口信息
  HWND target_window = nullptr;

  // 音频捕获（使用共享音频捕获模块）
  Utils::Media::AudioCapture::AudioCaptureContext audio;

  // 线程同步
  // encoder_write_mutex: 保护 sink_writer 的写入操作（视频帧回调和音频线程共享）
  std::mutex encoder_write_mutex;
  // resource_mutex: 保护资源的初始化、清理和帧填充逻辑（主线程独占）
  std::mutex resource_mutex;
};

}  // namespace Features::Recording::State
