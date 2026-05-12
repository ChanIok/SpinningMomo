module;

#include <wil/com.h>

module Features.Recording.Session;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Graphics.Capture;
import Utils.Graphics.CaptureRegion;
import Utils.Graphics.D3D;
import Utils.Logger;
import Utils.String;
import <d3d11_4.h>;
import <windows.h>;

namespace Features::Recording::Session {

auto floor_to_even(int value) -> int { return (value / 2) * 2; }

auto clear_queues(State::RecordingState& state) -> void {
  // 队列和通知标志只能在 queue_mutex 下动。音频线程、WGC 回调、编码线程都会碰它。
  std::lock_guard queue_lock(state.queue_mutex);
  state.audio_queue.clear();
  state.video_frame_pending = false;
}

auto clear_persistent_runtime_fields(State::RecordingState& state) -> void {
  state.winrt_device = nullptr;
  state.context = nullptr;
  state.device = nullptr;
  state.d3d_initialized = false;
}

auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                          int frame_height)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
  if (frame_width <= 0 || frame_height <= 0) {
    return std::unexpected("Invalid frame size");
  }

  Features::Recording::State::CapturePlan plan;
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
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
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

auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<Features::Recording::State::CapturePlan, std::string> {
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

auto clear_session_runtime_fields(State::RecordingState& state) -> void {
  // 清空一个录制段的会话态，不动可复用 D3D 设备。
  state.config = {};
  state.working_output_path.clear();
  state.target_window = nullptr;
  state.capture_plan = {};
  state.capture_session = {};
  state.cropped_texture = nullptr;
  state.encoder = {};
  state.start_qpc_100ns = 0;
  state.video_timeline = {};
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

auto cancel_cleanup_timer(State::RecordingState& state) -> void {
  if (state.cleanup_timer && state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
  }
}

auto start_cleanup_timer(State::RecordingState& state, std::function<void()> on_timeout) -> void {
  if (!state.d3d_initialized) {
    return;
  }

  if (!state.cleanup_timer) {
    state.cleanup_timer.emplace();
  }

  cancel_cleanup_timer(state);
  auto result =
      state.cleanup_timer->set_timeout(std::chrono::milliseconds(5000), std::move(on_timeout));
  if (!result) {
    Logger().warn("Failed to set recording D3D cleanup timer");
    return;
  }

  Logger().debug("Recording D3D cleanup timer started (5 seconds)");
}

auto cleanup_d3d_resources(State::RecordingState& state) -> void {
  cancel_cleanup_timer(state);
  clear_persistent_runtime_fields(state);
}

auto ensure_d3d_resources_ready(State::RecordingState& state) -> std::expected<void, std::string> {
  if (state.d3d_initialized && state.device && state.context && state.winrt_device) {
    return {};
  }

  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    cleanup_d3d_resources(state);
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
    cleanup_d3d_resources(state);
    return std::unexpected("Failed to create WinRT device: " + winrt_device_result.error());
  }

  state.winrt_device = *winrt_device_result;
  state.d3d_initialized = true;
  return {};
}

auto stop_capture_input(State::RecordingState& state) -> void {
  // 编码线程可能正在从 frame pool 取帧并直接写 SinkWriter；先互斥地停止产帧。
  std::lock_guard frame_lock(state.frame_mutex);
  Utils::Graphics::Capture::stop_capture_session(state.capture_session);
}

auto cleanup_capture_session(State::RecordingState& state) -> void {
  Utils::Graphics::Capture::cleanup_capture_session(state.capture_session);
}

}  // namespace Features::Recording::Session
