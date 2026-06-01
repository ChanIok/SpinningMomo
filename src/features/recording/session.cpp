module;

module Features.Recording.Session;

import std;
import Core.State;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Graphics.D3D;
import Utils.Logger;
import Utils.String;
import <d3d11_4.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::Session {

auto floor_to_even(int value) -> int { return (value / 2) * 2; }

auto clear_queues(State::RecordingState& state) -> void {
  // 队列和通知标志只能在 queue_mutex 下动。音频线程、WGC 回调、编码线程都会碰它。
  std::lock_guard queue_lock(state.queue_mutex);
  state.audio_queue.clear();
  state.video_frame_pending = false;
}

// 清空 D3D 等持久资源（录制段间不复用）
auto clear_persistent_runtime_fields(State::RecordingState& state) -> void {
  state.winrt_device = nullptr;
  state.context = nullptr;
  state.device = nullptr;
  state.d3d_initialized = false;
}

// 根据源帧尺寸和目标窗口算出输出尺寸和裁剪区域
auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                          int frame_height)
    -> std::expected<Features::Recording::Types::CapturePlan, std::string> {
  if (frame_width <= 0 || frame_height <= 0) {
    return std::unexpected("Invalid frame size");
  }

  Features::Recording::Types::CapturePlan plan;
  plan.source_width = frame_width;
  plan.source_height = frame_height;

  // 录整个窗口时基本不用算复杂裁剪，只要把宽高修成偶数。
  // 编码器通常不喜欢奇数宽高，尤其是 H.264/H.265。
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

  // 只录客户区时，WGC 给的是完整窗口画面，这里要把边框和标题栏裁掉。
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
    -> std::expected<Features::Recording::Types::CapturePlan, std::string> {
  auto capture_plan_result =
      resolve_capture_plan(target_window, config.capture_client_area, frame_width, frame_height);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  // 正常录制中，输出尺寸应该和启动时定下来的尺寸一致。
  // 如果窗口尺寸真的变了，交给 auto restart 切一个新文件，而不是硬塞进老编码器。
  if (capture_plan_result->output_width != config.width ||
      capture_plan_result->output_height != config.height) {
    return std::unexpected(std::format("Unexpected recording output size {}x{} (expected {}x{})",
                                       capture_plan_result->output_width,
                                       capture_plan_result->output_height, config.width,
                                       config.height));
  }

  return capture_plan_result;
}

// 录制启动时以 WGC 真实尺寸计算捕获计划，不用窗口矩形猜
auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<Features::Recording::Types::CapturePlan, std::string> {
  // 启动时以 WGC 的真实尺寸为准，不用窗口矩形猜。
  // 窗口边框、DPI、奇偶像素都可能让窗口矩形和实际帧差 1px。
  auto capture_size_result = Utils::Graphics::Capture::get_capture_item_size(target_window);
  if (!capture_size_result) {
    return std::unexpected(capture_size_result.error());
  }

  return resolve_capture_plan(target_window, capture_client_area, capture_size_result->first,
                              capture_size_result->second);
}

auto build_working_output_path(const std::filesystem::path& final_output_path)
    -> std::filesystem::path {
  auto working_output_path = final_output_path;
  // 录制没成功封尾前不直接写 .mp4，避免图库或播放器误以为这是一个完整视频。
  // 例如 final 是 20260511_xxx.mp4，这里会先写到 20260511_xxx。
  working_output_path.replace_extension();
  return working_output_path;
}

auto build_output_path_in_directory(const std::filesystem::path& output_directory)
    -> std::filesystem::path {
  auto filename = Utils::String::FormatTimestamp(std::chrono::system_clock::now());
  auto dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos) + ".mp4";
  } else {
    filename += ".mp4";
  }
  return output_directory / filename;
}

// finalize 成功后把临时文件 rename 为 .mp4，比复制更快
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

  // 只有编码线程 finalize 成功后才走到这里。
  // rename 比复制更快，也能减少半成品 .mp4 被看到的时间窗口。
  std::filesystem::rename(working_output_path, final_output_path, ec);
  if (ec) {
    return std::unexpected("Failed to move finalized recording to destination: " + ec.message());
  }

  return {};
}

auto delete_working_output_file(const std::filesystem::path& working_output_path,
                                std::string_view reason) -> void {
  if (working_output_path.empty()) {
    return;
  }

  std::error_code ec;
  if (!std::filesystem::exists(working_output_path, ec)) {
    if (ec) {
      Logger().warn("Failed to probe discarded recording file '{}': {}",
                    working_output_path.string(), ec.message());
    }
    return;
  }

  std::filesystem::remove(working_output_path, ec);
  if (ec) {
    Logger().warn("Failed to delete discarded recording file '{}': {}",
                  working_output_path.string(), ec.message());
    return;
  }

  Logger().info("Discarded recording file '{}': {}", working_output_path.string(), reason);
}

// 清空单段录制的会话态，但保留可复用的 D3D 设备，支持高频启停
auto clear_session_runtime_fields(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  // 清空一个录制段的会话态，不动可复用 D3D 设备。
  state.config = {};
  state.working_output_path.clear();
  state.target_window = nullptr;
  state.capture_plan = {};
  state.capture_session = {};
  state.cropped_texture = nullptr;
  state.encoder = {};
  state.start_qpc_100ns = 0;
  state.video_frame_interval_100ns = 10'000'000LL / 30;
  state.next_video_timestamp_100ns = -1;
  state.last_emitted_video_timestamp_100ns = -1;
  state.frozen_finish_target_100ns.store(-1, std::memory_order_release);
  state.encoder_input_texture = nullptr;
  state.has_encoder_input_texture = false;
  state.last_frame_width = 0;
  state.last_frame_height = 0;
  clear_queues(state);
  state.dropped_audio_packets.store(0, std::memory_order_release);
  state.encoded_video_frames = 0;
  state.encoded_audio_packets = 0;
  state.encoder_thread = {};
  state.accepting_input.store(false, std::memory_order_release);
  state.finish_requested.store(false, std::memory_order_release);
  state.has_audio.store(false, std::memory_order_release);
  state.encoder_ready = false;
  state.encoder_start_succeeded = false;
  state.finalize_succeeded = false;
  state.encoder_error.clear();
}

// 取消 D3D 延迟释放定时器（比如新的录制马上要开始，就不等了）
auto cancel_cleanup_timer(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  if (state.cleanup_timer && state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
  }
}

// 高频启停时延迟 5 秒释放 D3D 资源，避免反复创建的损耗
auto start_cleanup_timer(Core::State::AppState& app_state, std::function<void()> on_timeout)
    -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  if (!state.d3d_initialized) {
    return;
  }

  if (!state.cleanup_timer) {
    state.cleanup_timer.emplace();
  }

  cancel_cleanup_timer(app_state);
  auto result =
      state.cleanup_timer->set_timeout(std::chrono::milliseconds(5000), std::move(on_timeout));
  if (!result) {
    Logger().warn("Failed to set recording D3D cleanup timer");
    return;
  }

  Logger().debug("Recording D3D cleanup timer started (5 seconds)");
}

// 立即释放 D3D 设备（清理定时器 + 清空持久字段）
auto cleanup_d3d_resources(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  cancel_cleanup_timer(app_state);
  clear_persistent_runtime_fields(state);
}

// 确保录制所需 D3D device/context 和 WinRT device 已创建，没有就新建
auto ensure_d3d_resources_ready(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  if (!app_state.recording) {
    return std::unexpected("RecordingState is not initialized");
  }

  auto& state = *app_state.recording;

  if (state.d3d_initialized && state.device && state.context && state.winrt_device) {
    return {};
  }

  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    cleanup_d3d_resources(app_state);
    return std::unexpected("Failed to create D3D device: " + d3d_result.error());
  }
  state.device = d3d_result->first;
  state.context = d3d_result->second;

  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(state.device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    multithread->SetMultithreadProtected(TRUE);
  }

  auto winrt_device_result = Utils::Graphics::Capture::create_winrt_device(state.device.get());
  if (!winrt_device_result) {
    cleanup_d3d_resources(app_state);
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
  }

  state.winrt_device = *winrt_device_result;
  state.d3d_initialized = true;
  return {};
}

// 停止 WGC 继续产新帧，但保留 frame pool 供编码线程排空已到达的帧
auto stop_capture_input(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  std::lock_guard frame_lock(state.frame_mutex);
  Utils::Graphics::Capture::stop_capture_session(state.capture_session);
}

// 完全清理 WGC 捕获会话（释放所有关联资源）
auto cleanup_capture_session(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);
}

}  // namespace Features::Recording::Session
