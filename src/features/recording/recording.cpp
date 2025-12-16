module;

#include <d3d11_4.h>
#include <mfapi.h>
#include <wil/com.h>
#include <windows.h>
#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording;

import std;
import Features.Recording.State;
import Features.Recording.Encoder;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Utils.Logger;

namespace Features::Recording {

auto initialize(Features::Recording::State::RecordingState& state)
    -> std::expected<void, std::string> {
  // 可以在这里进行一些预初始化
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(Features::Recording::State::RecordingState& state,
                      Utils::Graphics::Capture::Direct3D11CaptureFrame frame) -> void {
  std::lock_guard lock(state.mutex);

  if (state.status != Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto frame_duration = std::chrono::microseconds(1'000'000 / state.config.fps);
  auto expected_time = state.start_time + state.frame_index * frame_duration;

  // 简单的帧率控制：如果还没到下一帧的时间，就跳过
  if (now < expected_time) {
    return;
  }

  // 获取纹理
  auto texture = Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(
      frame.Surface());
  if (!texture) {
    return;
  }

  // 计算时间戳 (100ns 单位)
  int64_t timestamp = state.frame_index * (10'000'000 / state.config.fps);

  // 编码
  auto result = Features::Recording::Encoder::encode_frame(state.encoder, state.context.get(),
                                                           texture.get(), timestamp,
                                                           state.config.fps);
  if (!result) {
    Logger().error("Failed to encode frame: {}", result.error());
    // 可以选择停止录制或记录错误
  } else {
    state.frame_index++;
  }
}

auto start(Features::Recording::State::RecordingState& state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  std::lock_guard lock(state.mutex);

  if (state.status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  state.config = config;
  state.target_window = target_window;

  // 1. 获取窗口尺寸
  RECT rect;
  GetClientRect(target_window, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;

  // 确保尺寸有效 (偶数)
  width = (width / 2) * 2;
  height = (height / 2) * 2;
  
  if (width <= 0 || height <= 0) {
      return std::unexpected("Invalid window size");
  }
  
  // 更新配置中的尺寸
  state.config.width = width;
  state.config.height = height;

  // 2. 创建 Headless D3D 设备
  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    return std::unexpected("Failed to create D3D device: " + d3d_result.error());
  }
  state.device = d3d_result->first;
  state.context = d3d_result->second;

  // 2.5. 启用 D3D11 多线程保护 (对 GPU 编码很重要)
  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(state.device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    multithread->SetMultithreadProtected(TRUE);
  }

  // 3. 创建 WinRT 设备
  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(state.device.get());
  if (!winrt_device_result) {
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
  }

  // 4. 创建编码器
  auto encoder_result = Features::Recording::Encoder::create_encoder(
      config.output_path, width, height, config.fps, config.bitrate, state.device.get(),
      config.encoder_mode);
  if (!encoder_result) {
    return std::unexpected("Failed to create encoder: " + encoder_result.error());
  }
  state.encoder = std::move(*encoder_result);

  // 5. 创建捕获会话
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, width, height,
      [&state](auto frame) { on_frame_arrived(state, frame); });

  if (!capture_result) {
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 6. 启动捕获
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  // 7. 更新状态
  state.start_time = std::chrono::steady_clock::now();
  state.frame_index = 0;
  state.status = Features::Recording::Types::RecordingStatus::Recording;

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

auto stop(Features::Recording::State::RecordingState& state) -> void {
  std::lock_guard lock(state.mutex);

  if (state.status != Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  state.status = Features::Recording::Types::RecordingStatus::Stopping;

  // 1. 停止捕获
  Utils::Graphics::Capture::stop_capture(state.capture_session);

  // 2. 完成编码
  auto finalize_result = Features::Recording::Encoder::finalize_encoder(state.encoder);
  if (!finalize_result) {
    Logger().error("Failed to finalize encoder: {}", finalize_result.error());
  }

  // 3. 清理资源
  state.capture_session = {};
  state.encoder = {};
  state.device = nullptr;
  state.context = nullptr;

  state.status = Features::Recording::Types::RecordingStatus::Idle;
  Logger().info("Recording stopped");
}

auto cleanup(Features::Recording::State::RecordingState& state) -> void {
  stop(state);
  MFShutdown();
}

}  // namespace Features::Recording
