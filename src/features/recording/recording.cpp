module;

#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording;

import std;
import Features.Recording.State;
import Features.Recording.AudioCapture;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Graphics.D3D;
import Utils.Media.Encoder;
import Utils.Media.Encoder.Types;
import Utils.Logger;
import Utils.String;
import <d3d11_4.h>;
import <dwmapi.h>;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording {

auto floor_to_even(int value) -> int { return (value / 2) * 2; }
auto start(Features::Recording::State::RecordingState& state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Features::Recording::State::RecordingState& state) -> void;

struct CaptureDimensions {
  int window_width = 0;
  int window_height = 0;
  int client_width = 0;
  int client_height = 0;
  int output_width = 0;
  int output_height = 0;
};

auto get_capture_window_rect(HWND target_window, RECT& window_rect) -> bool {
  return SUCCEEDED(DwmGetWindowAttribute(target_window, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
                                         sizeof(window_rect))) ||
         GetWindowRect(target_window, &window_rect);
}

auto calculate_capture_dimensions(HWND target_window, bool capture_client_area)
    -> std::expected<CaptureDimensions, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Target window is invalid");
  }

  RECT window_rect{};
  RECT client_rect{};
  if (!get_capture_window_rect(target_window, window_rect)) {
    return std::unexpected("Failed to get capture window rect");
  }
  if (!GetClientRect(target_window, &client_rect)) {
    return std::unexpected("Failed to get client rect");
  }

  CaptureDimensions dims;
  dims.window_width = window_rect.right - window_rect.left;
  dims.window_height = window_rect.bottom - window_rect.top;
  dims.client_width = client_rect.right - client_rect.left;
  dims.client_height = client_rect.bottom - client_rect.top;

  if (dims.window_width <= 0 || dims.window_height <= 0 || dims.client_width <= 0 ||
      dims.client_height <= 0) {
    return std::unexpected("Invalid window size");
  }

  dims.output_width = capture_client_area ? dims.client_width : dims.window_width;
  dims.output_height = capture_client_area ? dims.client_height : dims.window_height;
  dims.output_width = floor_to_even(dims.output_width);
  dims.output_height = floor_to_even(dims.output_height);

  if (dims.output_width <= 0 || dims.output_height <= 0) {
    return std::unexpected("Invalid output size");
  }

  return dims;
}

struct FrameCropPlan {
  bool should_crop = false;
  Utils::Graphics::CaptureRegion::CropRegion region{};
};

auto calculate_frame_crop_plan(HWND target_window,
                               const Features::Recording::Types::RecordingConfig& config,
                               int frame_width, int frame_height)
    -> std::expected<FrameCropPlan, std::string> {
  if (frame_width <= 0 || frame_height <= 0) {
    return std::unexpected("Invalid frame size");
  }

  FrameCropPlan plan;
  if (!config.capture_client_area) {
    if (frame_width != static_cast<int>(config.width) ||
        frame_height != static_cast<int>(config.height)) {
      return std::unexpected(
          std::format("Unexpected full-window capture frame size {}x{} (expected {}x{})",
                      frame_width, frame_height, config.width, config.height));
    }
    return plan;
  }

  if (frame_width == static_cast<int>(config.width) &&
      frame_height == static_cast<int>(config.height)) {
    return plan;
  }

  auto crop_region_result = Utils::Graphics::CaptureRegion::calculate_client_crop_region(
      target_window, static_cast<UINT>(frame_width), static_cast<UINT>(frame_height));
  if (!crop_region_result) {
    return std::unexpected("Failed to calculate client crop region: " + crop_region_result.error());
  }

  plan.should_crop = true;
  plan.region = *crop_region_result;
  return plan;
}

auto build_timestamp_output_path(const std::filesystem::path& reference_output_path)
    -> std::filesystem::path {
  auto filename = Utils::String::FormatTimestamp(std::chrono::system_clock::now());
  auto dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos) + ".mp4";
  } else {
    filename += ".mp4";
  }
  return reference_output_path.parent_path() / filename;
}

auto start_resize_restart_task(Features::Recording::State::RecordingState& state) -> void {
  bool expected = false;
  if (!state.resize_restart_in_progress.compare_exchange_strong(expected, true,
                                                                std::memory_order_acq_rel)) {
    return;
  }

  if (state.resize_restart_thread.joinable() &&
      state.resize_restart_thread.get_id() != std::this_thread::get_id()) {
    state.resize_restart_thread.join();
  }

  state.resize_restart_thread = std::jthread([&state](std::stop_token) {
    auto finish_task = [&state]() {
      state.resize_restart_in_progress.store(false, std::memory_order_release);
    };

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool need_uninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
      Logger().warn("CoInitializeEx failed in resize restart task: {:08X}",
                    static_cast<uint32_t>(hr));
    }

    try {
      if (state.status.load(std::memory_order_acquire) !=
          Features::Recording::Types::RecordingStatus::Recording) {
        finish_task();
        if (need_uninitialize) {
          CoUninitialize();
        }
        return;
      }

      Features::Recording::Types::RecordingConfig restart_config;
      HWND target_window = nullptr;
      {
        std::lock_guard resource_lock(state.resource_mutex);
        restart_config = state.config;
        target_window = state.target_window;
      }

      if (!target_window || !IsWindow(target_window)) {
        Logger().error("Skip recording auto restart after resize: target window is invalid");
        finish_task();
        if (need_uninitialize) {
          CoUninitialize();
        }
        return;
      }

      restart_config.output_path = build_timestamp_output_path(restart_config.output_path);
      Logger().info("Recording restarted with timestamp output after resize: {}",
                    restart_config.output_path.string());

      stop(state);
      auto restart_result = start(state, target_window, restart_config);
      if (!restart_result) {
        Logger().error("Failed to restart recording after resize: {}", restart_result.error());
      }

      finish_task();
      if (need_uninitialize) {
        CoUninitialize();
      }
    } catch (const std::exception& e) {
      Logger().error("Resize restart task exception: {}", e.what());
      finish_task();
      if (need_uninitialize) {
        CoUninitialize();
      }
    } catch (...) {
      Logger().error("Resize restart task exception: unknown");
      finish_task();
      if (need_uninitialize) {
        CoUninitialize();
      }
    }
  });
}

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
  // 使用 atomic 读取状态，无需上锁
  if (state.status.load(std::memory_order_acquire) !=
      Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto content_size = frame.ContentSize();
  if (content_size.Width > 0 && content_size.Height > 0) {
    bool frame_size_changed = content_size.Width != state.last_frame_width ||
                              content_size.Height != state.last_frame_height;

    if (frame_size_changed) {
      state.last_frame_width = content_size.Width;
      state.last_frame_height = content_size.Height;
      Utils::Graphics::Capture::recreate_frame_pool(state.capture_session, content_size.Width,
                                                    content_size.Height);

      if (state.config.auto_restart_on_resize) {
        auto dims_result =
            calculate_capture_dimensions(state.target_window, state.config.capture_client_area);
        if (dims_result) {
          if (dims_result->output_width != static_cast<int>(state.config.width) ||
              dims_result->output_height != static_cast<int>(state.config.height)) {
            start_resize_restart_task(state);
            return;
          }
        } else {
          Logger().warn("Skip recording auto restart after resize: {}", dims_result.error());
        }
      }
    }
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

  D3D11_TEXTURE2D_DESC source_desc{};
  texture->GetDesc(&source_desc);
  if (source_desc.Width == 0 || source_desc.Height == 0) {
    Logger().error("Failed to resolve recording source texture size");
    return;
  }

  // 填充缺失的帧（使用上一帧或当前帧重复）
  // 使用 resource_mutex 保护帧填充逻辑和 frame_index
  std::lock_guard resource_lock(state.resource_mutex);

  ID3D11Texture2D* current_texture = texture.get();
  auto crop_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                    static_cast<int>(source_desc.Width),
                                                    static_cast<int>(source_desc.Height));
  if (!crop_plan_result) {
    Logger().error("Failed to resolve recording crop plan: {}", crop_plan_result.error());
    return;
  }

  if (crop_plan_result->should_crop) {
    auto crop_result = Utils::Graphics::CaptureRegion::crop_texture_to_region(
        state.device.get(), state.context.get(), texture.get(), crop_plan_result->region,
        state.cropped_texture);
    if (!crop_result) {
      Logger().error("Failed to crop recording frame: {}", crop_result.error());
      return;
    }
    current_texture = *crop_result;
  }

  D3D11_TEXTURE2D_DESC current_desc{};
  current_texture->GetDesc(&current_desc);
  if (current_desc.Width != state.config.width || current_desc.Height != state.config.height) {
    Logger().error("Recording frame size mismatch after crop: got {}x{}, expected {}x{}",
                   current_desc.Width, current_desc.Height, state.config.width,
                   state.config.height);
    return;
  }

  while (state.frame_index <= target_frame_index) {
    int64_t timestamp = state.frame_index * frame_duration_100ns;

    // 选择要编码的纹理：如果有上一帧就用上一帧，否则用当前帧
    ID3D11Texture2D* encode_texture =
        (state.frame_index < target_frame_index && state.last_encoded_texture)
            ? state.last_encoded_texture.get()
            : current_texture;

    // 编码帧
    std::expected<void, std::string> result;
    {
      std::lock_guard write_lock(state.encoder_write_mutex);
      result = Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(),
                                                   encode_texture, timestamp, state.config.fps);
    }

    if (!result) {
      Logger().error("Failed to encode frame {}: {}", state.frame_index, result.error());
      // 编码失败时停止填充，避免连锁错误
      break;
    }

    state.frame_index++;
  }

  // 缓存当前帧作为下一次填充的参考
  // 首次调用时创建缓存纹理
  if (!state.last_encoded_texture) {
    D3D11_TEXTURE2D_DESC desc;
    current_texture->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;

    if (FAILED(state.device->CreateTexture2D(&desc, nullptr, state.last_encoded_texture.put()))) {
      Logger().error("Failed to create texture for frame caching");
      return;
    }
  }

  // 每帧都更新缓存（WGC 帧池会复用纹理，指针比较不可靠）
  state.context->CopyResource(state.last_encoded_texture.get(), current_texture);
}

auto start(Features::Recording::State::RecordingState& state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  // 使用 resource_mutex 保护资源初始化
  std::lock_guard resource_lock(state.resource_mutex);

  // 原子检查状态
  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  state.config = config;
  state.target_window = target_window;

  // 1. 计算捕获尺寸与输出尺寸
  auto dims_result = calculate_capture_dimensions(target_window, config.capture_client_area);
  if (!dims_result) {
    return std::unexpected(dims_result.error());
  }
  const auto& dims = *dims_result;
  int window_width = dims.window_width;
  int window_height = dims.window_height;
  int output_width = dims.output_width;
  int output_height = dims.output_height;

  state.config.width = output_width;
  state.config.height = output_height;
  state.last_frame_width = window_width;
  state.last_frame_height = window_height;
  state.cropped_texture = nullptr;

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

  // 4. 初始化音频捕获
  WAVEFORMATEX* wave_format = nullptr;

  // 获取目标窗口的进程 ID
  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  auto audio_result =
      Features::Recording::AudioCapture::initialize(state.audio, config.audio_source, process_id);
  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    wave_format = state.audio.wave_format;
    Logger().info("Audio capture initialized");
  }

  // 5. 创建编码器（音频流在内部添加）
  Utils::Media::Encoder::Types::EncoderConfig encoder_config;
  encoder_config.output_path = config.output_path;
  encoder_config.width = output_width;
  encoder_config.height = output_height;
  encoder_config.fps = config.fps;
  encoder_config.bitrate = config.bitrate;
  encoder_config.quality = config.quality;
  encoder_config.qp = config.qp;
  encoder_config.keyframe_interval = 2;  // 录制默认 2s 关键帧间隔
  encoder_config.rate_control = Utils::Media::Encoder::Types::rate_control_mode_from_string(
      Features::Recording::Types::rate_control_mode_to_string(config.rate_control));
  encoder_config.encoder_mode = Utils::Media::Encoder::Types::encoder_mode_from_string(
      Features::Recording::Types::encoder_mode_to_string(config.encoder_mode));
  encoder_config.codec = Utils::Media::Encoder::Types::video_codec_from_string(
      Features::Recording::Types::video_codec_to_string(config.codec));
  encoder_config.audio_bitrate = config.audio_bitrate;

  auto encoder_result =
      Utils::Media::Encoder::create_encoder(encoder_config, state.device.get(), wave_format);
  if (!encoder_result) {
    return std::unexpected("Failed to create encoder: " + encoder_result.error());
  }
  state.encoder = std::move(*encoder_result);

  Utils::Graphics::Capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;

  // 6. 创建捕获会话（使用 2 帧缓冲以容忍编码延迟）
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, window_width, window_height,
      [&state](auto frame) { on_frame_arrived(state, frame); }, 2, capture_options);

  if (!capture_result) {
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 7. 启动捕获
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  // 8. 更新状态
  state.start_time = std::chrono::steady_clock::now();
  state.frame_index = 0;

  // 9. 启动音频捕获线程（如果有音频）
  if (state.encoder.has_audio) {
    Features::Recording::AudioCapture::start_capture_thread(state);
  }

  // 10. 原子设置状态为 Recording（最后设置，确保所有资源就绪）
  state.status.store(Features::Recording::Types::RecordingStatus::Recording,
                     std::memory_order_release);

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

auto stop(Features::Recording::State::RecordingState& state) -> void {
  // 阶段1: 原子检查并设置状态为 Stopping（无需锁）
  auto expected = Features::Recording::Types::RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::Recording::Types::RecordingStatus::Stopping,
                                            std::memory_order_acq_rel)) {
    // 不是 Recording 状态，直接返回
    return;
  }

  // 阶段2: 通知并等待线程退出（无需锁，避免死锁）
  // 1. 停止音频捕获线程
  if (state.encoder.has_audio) {
    Features::Recording::AudioCapture::stop(state.audio);
  }

  // 2. 停止视频捕捉
  Utils::Graphics::Capture::stop_capture(state.capture_session);

  // 阶段3: 清理资源（使用 resource_mutex 保护）
  {
    std::lock_guard resource_lock(state.resource_mutex);

    // 3. 填充最后的视频帧（确保录制时长完整）
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
        std::expected<void, std::string> result;
        {
          std::lock_guard write_lock(state.encoder_write_mutex);
          result = Utils::Media::Encoder::encode_frame(state.encoder, state.context.get(),
                                                       state.last_encoded_texture.get(), timestamp,
                                                       state.config.fps);
        }

        if (!result) {
          Logger().error("Failed to encode final frame {}: {}", state.frame_index, result.error());
          break;
        }
        state.frame_index++;
      }

      Logger().info("Filled {} total frames for recording duration", state.frame_index);
    }

    // 4. 完成编码
    auto finalize_result = Utils::Media::Encoder::finalize_encoder(state.encoder);
    if (!finalize_result) {
      Logger().error("Failed to finalize encoder: {}", finalize_result.error());
    }

    // 5. 清理资源
    state.capture_session = {};
    state.encoder = {};
    state.last_encoded_texture = nullptr;
    state.last_frame_width = 0;
    state.last_frame_height = 0;
    state.cropped_texture = nullptr;
    state.device = nullptr;
    state.context = nullptr;

    // 6. 清理音频资源
    Features::Recording::AudioCapture::cleanup(state.audio);
  }

  // 阶段4: 原子设置最终状态
  state.status.store(Features::Recording::Types::RecordingStatus::Idle, std::memory_order_release);
  Logger().info("Recording stopped");
}

auto cleanup(Features::Recording::State::RecordingState& state) -> void {
  stop(state);
  MFShutdown();
}

}  // namespace Features::Recording
