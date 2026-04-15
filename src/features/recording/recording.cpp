module;

#include <winrt/Windows.Graphics.Capture.h>

module Features.Recording;

import std;
import Core.State;
import Features.Recording.State;
import Features.Recording.AudioCapture;
import UI.FloatingWindow;
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
auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Features::Recording::State::RecordingState& state) -> void;

// 统一几何入口：
// 无论录整窗还是客户区，都先把“源帧多大、最终编码多大、是否需要裁剪”
// 收敛成同一份 CapturePlan，避免 start / frame callback / resize 三处各算各的。
auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                          int frame_height)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  if (frame_width <= 0 || frame_height <= 0) {
    return std::unexpected("Invalid frame size");
  }

  Features::Recording::State::CapturePlan plan;
  plan.source_width = frame_width;
  plan.source_height = frame_height;

  if (!capture_client_area) {
    auto output_width = floor_to_even(frame_width);
    auto output_height = floor_to_even(frame_height);
    if (output_width <= 0 || output_height <= 0) {
      return std::unexpected("Resolved full-window output size is invalid");
    }

    plan.output_width = static_cast<std::uint32_t>(output_width);
    plan.output_height = static_cast<std::uint32_t>(output_height);
    plan.should_crop = output_width != frame_width || output_height != frame_height;
    plan.region = {
        .left = 0,
        .top = 0,
        .width = static_cast<UINT>(output_width),
        .height = static_cast<UINT>(output_height),
    };
    return plan;
  }

  auto crop_region_result = Utils::Graphics::CaptureRegion::calculate_client_crop_region(
      target_window, static_cast<UINT>(frame_width), static_cast<UINT>(frame_height));
  if (!crop_region_result) {
    return std::unexpected("Failed to calculate client crop region: " + crop_region_result.error());
  }

  auto region = *crop_region_result;
  auto output_width = floor_to_even(static_cast<int>(region.width));
  auto output_height = floor_to_even(static_cast<int>(region.height));
  if (output_width <= 0 || output_height <= 0) {
    return std::unexpected("Resolved client-area output size is invalid");
  }

  region.width = static_cast<UINT>(output_width);
  region.height = static_cast<UINT>(output_height);

  plan.output_width = static_cast<std::uint32_t>(output_width);
  plan.output_height = static_cast<std::uint32_t>(output_height);
  plan.should_crop = region.left != 0 || region.top != 0 || output_width != frame_width ||
                     output_height != frame_height;
  plan.region = region;
  return plan;
}

auto calculate_frame_crop_plan(HWND target_window,
                               const Features::Recording::Types::RecordingConfig& config,
                               int frame_width, int frame_height)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  auto capture_plan_result =
      resolve_capture_plan(target_window, config.capture_client_area, frame_width, frame_height);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  if (capture_plan_result->output_width != config.width ||
      capture_plan_result->output_height != config.height) {
    return std::unexpected(std::format("Unexpected recording output size {}x{} (expected {}x{})",
                                       capture_plan_result->output_width,
                                       capture_plan_result->output_height, config.width,
                                       config.height));
  }

  return capture_plan_result;
}

auto build_working_output_path(const std::filesystem::path& final_output_path)
    -> std::filesystem::path {
  // 录制过程中先写入“无后缀”的中间文件，只有 finalize 成功后才补上 .mp4。
  auto working_output_path = final_output_path;
  working_output_path.replace_extension();
  return working_output_path;
}

auto rename_working_output_to_final(const std::filesystem::path& working_output_path,
                                    const std::filesystem::path& final_output_path)
    -> std::expected<void, std::string> {
  if (working_output_path.empty() || final_output_path.empty() ||
      working_output_path == final_output_path) {
    return {};
  }

  std::error_code ec;
  if (std::filesystem::exists(final_output_path, ec)) {
    if (ec) {
      return std::unexpected("Failed to probe final output path: " + ec.message());
    }

    std::filesystem::remove(final_output_path, ec);
    if (ec) {
      return std::unexpected("Failed to remove existing final output file: " + ec.message());
    }
  }

  std::filesystem::rename(working_output_path, final_output_path, ec);
  if (ec) {
    return std::unexpected("Failed to move finalized recording to destination: " + ec.message());
  }

  return {};
}

auto clear_runtime_resources(Features::Recording::State::RecordingState& state) -> void {
  state.capture_session = {};
  state.encoder = {};
  state.capture_plan = {};
  state.working_output_path.clear();
  state.last_encoded_texture = nullptr;
  state.last_frame_width = 0;
  state.last_frame_height = 0;
  state.cropped_texture = nullptr;
  state.device = nullptr;
  state.context = nullptr;
}

auto clear_runtime_resources_after_failed_start(Features::Recording::State::RecordingState& state)
    -> void {
  auto working_output_path = state.working_output_path;
  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);
  state.encoder = {};
  state.capture_plan = {};
  state.working_output_path.clear();
  state.last_encoded_texture = nullptr;
  state.last_frame_width = 0;
  state.last_frame_height = 0;
  state.cropped_texture = nullptr;
  state.device = nullptr;
  state.context = nullptr;
  Features::Recording::AudioCapture::cleanup(state.audio);

  if (!working_output_path.empty()) {
    std::error_code ec;
    std::filesystem::remove(working_output_path, ec);
  }
}

auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  // 启动阶段优先相信 WGC 自己报告的捕获尺寸，而不是窗口矩形的推算结果。
  auto capture_size_result = Utils::Graphics::Capture::get_capture_item_size(target_window);
  if (!capture_size_result) {
    return std::unexpected(capture_size_result.error());
  }

  return resolve_capture_plan(target_window, capture_client_area, capture_size_result->first,
                              capture_size_result->second);
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

auto start_resize_restart_task(Core::State::AppState& app_state,
                               Features::Recording::State::RecordingState& state) -> void {
  bool expected = false;
  if (!state.resize_restart_in_progress.compare_exchange_strong(expected, true,
                                                                std::memory_order_acq_rel)) {
    return;
  }

  if (state.resize_restart_thread.joinable() &&
      state.resize_restart_thread.get_id() != std::this_thread::get_id()) {
    state.resize_restart_thread.join();
  }

  state.resize_restart_thread = std::jthread([&app_state, &state](std::stop_token) {
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
      std::lock_guard control_lock(state.control_mutex);

      if (state.shutdown_requested.load(std::memory_order_acquire)) {
        finish_task();
        if (need_uninitialize) {
          CoUninitialize();
        }
        return;
      }

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
      auto restart_result = start(app_state, state, target_window, restart_config);
      if (!restart_result) {
        Logger().error("Failed to restart recording after resize: {}", restart_result.error());
      } else {
        UI::FloatingWindow::request_repaint(app_state);
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

auto on_frame_arrived(Core::State::AppState& app_state,
                      Features::Recording::State::RecordingState& state,
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
      Utils::Graphics::Capture::recreate_frame_pool(state.capture_session, content_size.Width,
                                                    content_size.Height);

      // 这里先按“新窗口尺寸”直接重算新 plan；
      // 如果新 plan 的输出尺寸变了，后面的逻辑会决定自动切段，
      // 不能在这里先拿新尺寸去和当前旧录制段做硬校验。
      auto capture_plan_result =
          resolve_capture_plan(state.target_window, state.config.capture_client_area,
                               content_size.Width, content_size.Height);
      if (!capture_plan_result) {
        Logger().error("Failed to resolve recording crop plan after resize: {}",
                       capture_plan_result.error());
        return;
      }

      state.last_frame_width = content_size.Width;
      state.last_frame_height = content_size.Height;

      if (state.config.auto_restart_on_resize &&
          (capture_plan_result->output_width != state.config.width ||
           capture_plan_result->output_height != state.config.height)) {
        // 输出尺寸真的变化了：当前编码器已经不再适配，切段重开新文件。
        start_resize_restart_task(app_state, state);
        return;
      }

      {
        std::lock_guard resource_lock(state.resource_mutex);
        if (state.status.load(std::memory_order_acquire) !=
            Features::Recording::Types::RecordingStatus::Recording) {
          return;
        }

        state.capture_plan = *capture_plan_result;
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
  auto capture_plan = state.capture_plan;
  if (capture_plan.output_width == 0 || capture_plan.output_height == 0) {
    auto capture_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                         static_cast<int>(source_desc.Width),
                                                         static_cast<int>(source_desc.Height));
    if (!capture_plan_result) {
      Logger().error("Failed to resolve recording crop plan: {}", capture_plan_result.error());
      return;
    }
    state.capture_plan = *capture_plan_result;
    capture_plan = *capture_plan_result;
  }

  if (capture_plan.source_width != static_cast<int>(source_desc.Width) ||
      capture_plan.source_height != static_cast<int>(source_desc.Height)) {
    auto capture_plan_result = calculate_frame_crop_plan(state.target_window, state.config,
                                                         static_cast<int>(source_desc.Width),
                                                         static_cast<int>(source_desc.Height));
    if (!capture_plan_result) {
      Logger().error("Failed to refresh recording crop plan: {}", capture_plan_result.error());
      return;
    }
    state.capture_plan = *capture_plan_result;
    capture_plan = *capture_plan_result;
  }

  if (capture_plan.output_width == 0 || capture_plan.output_height == 0) {
    Logger().error("Recording capture plan is invalid");
    return;
  }

  if (capture_plan.should_crop) {
    auto crop_result = Utils::Graphics::CaptureRegion::crop_texture_to_region(
        state.device.get(), state.context.get(), texture.get(), capture_plan.region,
        state.cropped_texture);
    if (!crop_result) {
      Logger().error("Failed to crop recording frame: {}", crop_result.error());
      return;
    }
    current_texture = *crop_result;
  }

  D3D11_TEXTURE2D_DESC current_desc{};
  current_texture->GetDesc(&current_desc);
  if (current_desc.Width != capture_plan.output_width ||
      current_desc.Height != capture_plan.output_height) {
    Logger().error("Recording frame size mismatch after crop: got {}x{}, expected {}x{}",
                   current_desc.Width, current_desc.Height, capture_plan.output_width,
                   capture_plan.output_height);
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

auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
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
  state.working_output_path = build_working_output_path(config.output_path);

  // 1. 基于 WGC 实际源尺寸解析统一的捕获计划。
  // 后面 encoder/session/frame callback 都围绕这份 plan 工作。
  auto capture_plan_result = build_startup_capture_plan(target_window, config.capture_client_area);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }
  const auto& capture_plan = *capture_plan_result;
  int source_width = capture_plan.source_width;
  int source_height = capture_plan.source_height;
  int output_width = static_cast<int>(capture_plan.output_width);
  int output_height = static_cast<int>(capture_plan.output_height);

  state.config.width = output_width;
  state.config.height = output_height;
  state.capture_plan = capture_plan;
  state.last_frame_width = source_width;
  state.last_frame_height = source_height;
  state.cropped_texture = nullptr;
  state.last_encoded_texture = nullptr;

  // 2. 创建 Headless D3D 设备
  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    state.working_output_path.clear();
    state.capture_plan = {};
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
    clear_runtime_resources_after_failed_start(state);
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

  // 5. 创建编码器（音频流在内部添加）。
  // 注意这里写的是无后缀的中间路径，不是最终对外可见的 .mp4。
  Utils::Media::Encoder::Types::EncoderConfig encoder_config;
  encoder_config.output_path = state.working_output_path;
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
    clear_runtime_resources_after_failed_start(state);
    return std::unexpected("Failed to create encoder: " + encoder_result.error());
  }
  state.encoder = std::move(*encoder_result);

  Utils::Graphics::Capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;

  // 6. 创建捕获会话（使用 2 帧缓冲以容忍编码延迟）
  auto capture_result = Utils::Graphics::Capture::create_capture_session(
      target_window, *winrt_device_result, source_width, source_height,
      [&app_state, &state](auto frame) { on_frame_arrived(app_state, state, frame); }, 2,
      capture_options);

  if (!capture_result) {
    clear_runtime_resources_after_failed_start(state);
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 7. 启动捕获
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    clear_runtime_resources_after_failed_start(state);
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

  const auto final_output_path = state.config.output_path;
  const auto working_output_path = state.working_output_path;
  bool finalize_succeeded = false;

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
    } else {
      finalize_succeeded = true;
    }

    // 5. 清理资源
    clear_runtime_resources(state);

    // 6. 清理音频资源
    Features::Recording::AudioCapture::cleanup(state.audio);
  }

  if (finalize_succeeded) {
    // 只有在容器收尾成功后，才把临时文件发布为最终成品。
    auto rename_result = rename_working_output_to_final(working_output_path, final_output_path);
    if (!rename_result) {
      Logger().error("Failed to publish finalized recording '{}': {}", final_output_path.string(),
                     rename_result.error());
    }
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
