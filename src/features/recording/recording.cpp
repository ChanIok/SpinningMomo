module;

#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording;

import std;
import Features.Recording.State;
import Features.Recording.Encoder;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Utils.Logger;
import <d3d11_4.h>;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

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

  // 获取当前时间和帧的系统时间戳
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - state.start_time;
  auto elapsed_100ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

  // 计算理论上应该编码到第几帧
  int64_t frame_duration_100ns = 10'000'000 / state.config.fps;  // 每帧的时长（100ns单位）
  int64_t target_frame_index = elapsed_100ns / frame_duration_100ns;

  // 获取当前帧纹理
  auto texture =
      Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(frame.Surface());
  if (!texture) {
    Logger().error("Failed to get texture from capture frame");
    return;
  }

  // 填充缺失的帧（使用上一帧或当前帧重复）
  while (state.frame_index <= target_frame_index) {
    int64_t timestamp = state.frame_index * frame_duration_100ns;

    // 选择要编码的纹理：如果有上一帧就用上一帧，否则用当前帧
    ID3D11Texture2D* encode_texture =
        (state.frame_index < target_frame_index && state.last_encoded_texture)
            ? state.last_encoded_texture.get()
            : texture.get();

    // 编码帧
    auto result = Features::Recording::Encoder::encode_frame(
        state.encoder, state.context.get(), encode_texture, timestamp, state.config.fps);

    if (!result) {
      Logger().error("Failed to encode frame {}: {}", state.frame_index, result.error());
      // 编码失败时停止填充，避免连锁错误
      break;
    }

    state.frame_index++;
  }

  // 缓存当前帧作为下一次填充的参考
  // 只有当纹理不同时才更新（避免不必要的复制）
  if (!state.last_encoded_texture || state.last_encoded_texture.get() != texture.get()) {
    // 创建一个新的纹理副本用于缓存
    if (!state.last_encoded_texture) {
      D3D11_TEXTURE2D_DESC desc;
      texture->GetDesc(&desc);
      desc.BindFlags = 0;
      desc.MiscFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.CPUAccessFlags = 0;

      if (FAILED(state.device->CreateTexture2D(&desc, nullptr, state.last_encoded_texture.put()))) {
        Logger().error("Failed to create texture for frame caching");
        return;
      }
    }

    state.context->CopyResource(state.last_encoded_texture.get(), texture.get());
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
      config.encoder_mode, config.codec, config.rate_control, config.quality, config.qp);
  if (!encoder_result) {
    return std::unexpected("Failed to create encoder: " + encoder_result.error());
  }
  state.encoder = std::move(*encoder_result);

  // 5. 创建捕获会话（使用 2 帧缓冲以容忍编码延迟）
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, width, height,
      [&state](auto frame) { on_frame_arrived(state, frame); }, 2);

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

  // 1. 停止捕捉
  Utils::Graphics::Capture::stop_capture(state.capture_session);

  // 2. 填充最后的帧（确保录制时长完整）
  if (state.last_encoded_texture) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - state.start_time;
    auto elapsed_100ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;

    int64_t frame_duration_100ns = 10'000'000 / state.config.fps;
    int64_t final_frame_index = elapsed_100ns / frame_duration_100ns;

    // 用最后一帧填充到结束
    while (state.frame_index <= final_frame_index) {
      int64_t timestamp = state.frame_index * frame_duration_100ns;
      auto result = Features::Recording::Encoder::encode_frame(state.encoder, state.context.get(),
                                                               state.last_encoded_texture.get(),
                                                               timestamp, state.config.fps);

      if (!result) {
        Logger().error("Failed to encode final frame {}: {}", state.frame_index, result.error());
        break;
      }
      state.frame_index++;
    }

    Logger().info("Filled {} total frames for recording duration", state.frame_index);
  }

  // 3. 完成编码
  auto finalize_result = Features::Recording::Encoder::finalize_encoder(state.encoder);
  if (!finalize_result) {
    Logger().error("Failed to finalize encoder: {}", finalize_result.error());
  }

  // 4. 清理资源
  state.capture_session = {};
  state.encoder = {};
  state.last_encoded_texture = nullptr;
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
