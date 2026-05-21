module;

#include <wil/com.h>

module Features.Recording;

import std;
import Core.State;
import Features.Recording.AudioCapture;
import Features.Recording.EncoderLoop;
import Features.Recording.Session;
import Features.Recording.State;
import Features.Recording.Types;
import UI.FloatingWindow;
import Utils.Graphics.Capture;
import Utils.Logger;
import <mfapi.h>;
import <windows.h>;

namespace Features::Recording {

// 录制模块的大致数据流：
// WGC 帧回调只负责唤醒编码线程；音频采集线程只复制 PCM 数据并入队；
// 编码线程按固定 fps 消费捕获（每次最多取一帧 Copy 到自有纹理），是唯一写 SinkWriter 的地方；
// 控制线程负责把 start / stop / resize restart 串起来，避免多个重操作互相打架。
auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }

  constexpr long double kHundredNsPerSecond = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * kHundredNsPerSecond / frequency.QuadPart);
}

auto start(Core::State::AppState& app_state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Core::State::AppState& app_state) -> void;
auto request_control_action(Core::State::AppState& app_state,
                            Features::Recording::Types::RecordingControlAction action) -> bool;

auto request_control_action(Core::State::AppState& app_state,
                            Features::Recording::Types::RecordingControlAction action) -> bool {
  if (!app_state.recording) {
    return false;
  }

  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire) &&
      action != Types::RecordingControlAction::ShutdownStop) {
    return false;
  }

  {
    std::lock_guard request_lock(state.control_request_mutex);

    // shutdown 优先级最高。退出时不再接受普通 toggle / resize restart。
    if (action == Types::RecordingControlAction::ShutdownStop) {
      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // 用户快速连按时，只保留一个 toggle。
    // 如果当前已经在 start/stop，新的 toggle 直接忽略，避免重入。
    if (action == Types::RecordingControlAction::Toggle) {
      if (state.control_action_running.load(std::memory_order_acquire) ||
          state.pending_action != Types::RecordingControlAction::None) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // resize restart 可以被后来的 resize 覆盖，但不能插到 shutdown 或用户 toggle 前面。
    if (action == Types::RecordingControlAction::RestartAfterResize) {
      if (state.pending_action == Types::RecordingControlAction::ShutdownStop ||
          state.pending_action == Types::RecordingControlAction::Toggle) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    if (action == Types::RecordingControlAction::CleanupD3D) {
      if (state.pending_action == Types::RecordingControlAction::ShutdownStop ||
          state.pending_action == Types::RecordingControlAction::Toggle ||
          state.pending_action == Types::RecordingControlAction::RestartAfterResize) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }
  }

  return false;
}

auto handle_control_action(Core::State::AppState& app_state,
                           Features::Recording::State::RecordingState& state,
                           const RecordingControlHandlers& handlers,
                           Features::Recording::Types::RecordingControlAction action) -> bool {
  switch (action) {
    case Types::RecordingControlAction::Toggle:
      if (handlers.on_toggle) {
        handlers.on_toggle();
      }
      return true;

    case Types::RecordingControlAction::RestartAfterResize:
      restart_after_resize(app_state);
      return true;

    case Types::RecordingControlAction::ShutdownStop:
      if (handlers.on_shutdown_stop) {
        handlers.on_shutdown_stop();
      }
      return false;

    case Types::RecordingControlAction::CleanupD3D:
      if (state.status.load(std::memory_order_acquire) ==
          Features::Recording::Types::RecordingStatus::Idle) {
        Features::Recording::Session::cleanup_d3d_resources(app_state);
        Logger().debug("Recording reusable D3D resources cleaned up");
      }
      return true;

    default:
      return true;
  }
}

auto control_thread_proc(Core::State::AppState& app_state,
                         Features::Recording::State::RecordingState& state,
                         RecordingControlHandlers handlers, std::stop_token stop_token) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    std::stop_callback wake_on_stop(stop_token, [&state]() { state.control_cv.notify_all(); });

    while (true) {
      Types::RecordingControlAction action{Types::RecordingControlAction::None};

      {
        std::unique_lock request_lock(state.control_request_mutex);
        // 控制线程平时睡在这里。有 toggle / resize / shutdown 请求时醒来处理。
        state.control_cv.wait(request_lock, [&]() {
          return stop_token.stop_requested() ||
                 state.pending_action != Types::RecordingControlAction::None;
        });

        if (stop_token.stop_requested() &&
            state.pending_action == Types::RecordingControlAction::None) {
          break;
        }

        // 取出请求后立刻清空槽位。真正执行 start/stop 时不拿这个锁，
        // 否则 UI 线程提交 shutdown 请求可能被长时间卡住。
        action = state.pending_action;
        state.pending_action = Types::RecordingControlAction::None;
        state.control_action_running.store(true, std::memory_order_release);
      }

      bool keep_running = true;
      try {
        keep_running = handle_control_action(app_state, state, handlers, action);
      } catch (const std::exception& e) {
        Logger().error("Recording control thread action exception: {}", e.what());
      } catch (...) {
        Logger().error("Recording control thread action exception: unknown");
      }

      state.control_action_running.store(false, std::memory_order_release);
      if (!keep_running) {
        break;
      }
    }
  } catch (const wil::ResultException& e) {
    Logger().error("Recording control thread COM initialization failed: {} (HRESULT: 0x{:08X})",
                   e.what(), static_cast<unsigned>(e.GetErrorCode()));
    state.control_action_running.store(false, std::memory_order_release);
  } catch (const std::exception& e) {
    Logger().error("Recording control thread exception: {}", e.what());
    state.control_action_running.store(false, std::memory_order_release);
  } catch (...) {
    Logger().error("Recording control thread exception: unknown");
    state.control_action_running.store(false, std::memory_order_release);
  }
}

auto ensure_control_thread_started(Core::State::AppState& app_state,
                                   RecordingControlHandlers handlers)
    -> std::expected<void, std::string> {
  if (!app_state.recording) {
    return std::unexpected("RecordingState is not initialized");
  }

  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  if (state.control_thread.joinable()) {
    return {};
  }

  state.control_thread = std::jthread(
      [&app_state, handlers = std::move(handlers)](std::stop_token stop_token) mutable {
        if (!app_state.recording) {
          return;
        }

        auto& rec = *app_state.recording;
        control_thread_proc(app_state, rec, std::move(handlers), stop_token);
      });
  return {};
}

auto join_control_thread(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  if (state.control_thread.joinable()) {
    state.control_thread.join();
  }
}

auto request_restart_after_resize(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  request_control_action(app_state, Types::RecordingControlAction::RestartAfterResize);
}

auto restart_after_resize(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  if (state.status.load(std::memory_order_acquire) !=
      Features::Recording::Types::RecordingStatus::Recording) {
    return;
  }

  auto restart_config = state.config;
  auto target_window = state.target_window;

  if (!target_window || !IsWindow(target_window)) {
    Logger().error("Skip recording auto restart after resize: target window is invalid");
    return;
  }

  // 尺寸变化后不能继续写进旧编码器，所以保存当前段，再用新尺寸开一个新段。
  restart_config.output_path =
      Features::Recording::Session::build_timestamp_output_path(restart_config.output_path);
  Logger().info("Recording restarted with timestamp output after resize: {}",
                restart_config.output_path.string());

  stop(app_state);
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  auto restart_result = start(app_state, target_window, restart_config);
  if (!restart_result) {
    Logger().error("Failed to restart recording after resize: {}", restart_result.error());
  } else {
    UI::FloatingWindow::request_repaint(app_state);
  }
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  (void)app_state;
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(Core::State::AppState& app_state) -> void {
  Features::Recording::EncoderLoop::mark_video_frame_pending(app_state);
}

auto cleanup_failed_start(Core::State::AppState& app_state, std::string_view reason) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  // start 中途失败也走一遍“停输入 -> 停止产帧 -> 停编码线程 -> 清资源”。
  // 这样失败路径和正常 stop 的资源顺序保持一致。
  state.accepting_input.store(false, std::memory_order_release);
  Features::Recording::Session::stop_capture_input(app_state);
  Features::Recording::AudioCapture::stop(state.audio);
  Features::Recording::EncoderLoop::signal_encoder_finish(app_state);

  if (state.encoder_thread.joinable()) {
    state.encoder_thread.request_stop();
    state.encoder_thread.join();
  }

  Features::Recording::AudioCapture::cleanup(state.audio);
  Features::Recording::Session::cleanup_capture_session(app_state);
  Features::Recording::Session::delete_working_output_file(state.working_output_path, reason);
  Features::Recording::Session::clear_session_runtime_fields(app_state);
}

auto start(Core::State::AppState& app_state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  if (!app_state.recording) {
    return std::unexpected("RecordingState is not initialized");
  }

  auto& state = *app_state.recording;

  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  Features::Recording::Session::clear_session_runtime_fields(app_state);
  Features::Recording::Session::cancel_cleanup_timer(app_state);

  // 先算清楚这次录制的源尺寸和输出尺寸。编码器创建后，宽高就不能再改。
  auto capture_plan_result = Features::Recording::Session::build_startup_capture_plan(
      target_window, config.capture_client_area);
  if (!capture_plan_result) {
    return std::unexpected(capture_plan_result.error());
  }

  state.config = config;
  state.target_window = target_window;
  state.working_output_path =
      Features::Recording::Session::build_working_output_path(config.output_path);
  state.capture_plan = *capture_plan_result;
  state.last_frame_width = capture_plan_result->source_width;
  state.last_frame_height = capture_plan_result->source_height;
  state.config.width = static_cast<std::uint32_t>(capture_plan_result->output_width);
  state.config.height = static_cast<std::uint32_t>(capture_plan_result->output_height);
  // 其余视频时钟 / 纹理初值由 clear_session_runtime_fields 已清空；这里只按本段 fps 覆盖帧间隔。
  const auto fps = std::max<std::uint32_t>(state.config.fps, 1);
  state.video_frame_interval_100ns = std::max<std::int64_t>(1, 10'000'000LL / fps);

  // 录制内复用 D3D 设备，避免高频启停反复初始化。
  auto d3d_ready_result = Features::Recording::Session::ensure_d3d_resources_ready(app_state);
  if (!d3d_ready_result) {
    Features::Recording::Session::clear_session_runtime_fields(app_state);
    return d3d_ready_result;
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  auto audio_result =
      Features::Recording::AudioCapture::initialize(state.audio, config.audio_source, process_id);
  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    Logger().info("Audio capture initialized");
  }

  // 先启动编码线程并等它创建好 SinkWriter，再开始 WGC 捕获。
  // 否则捕获已经在产帧，但编码器可能还没准备好。
  state.encoder_thread = std::jthread([&app_state](std::stop_token stop_token) {
    Features::Recording::EncoderLoop::encoder_thread_proc(
        app_state, stop_token, [&app_state]() { request_restart_after_resize(app_state); });
  });
  Features::Recording::EncoderLoop::wait_encoder_ready(app_state);

  if (!state.encoder_start_succeeded) {
    std::string error =
        state.encoder_error.empty() ? "Failed to start recording encoder" : state.encoder_error;
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected(error);
  }

  Utils::Graphics::Capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;
  if (config.enable_hdr) {
    // HDR 桌面捕获使用 scRGB 16-bit float。后续编码线程会把它转换成 HEVC 编码器需要的 P010。
    capture_options.pixel_format =
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // WGC 回调只通知编码线程；真正取帧和写编码器都在编码线程里完成。
  auto capture_result = Utils::Graphics::Capture::create_capture_session_with_frame_notification(
      target_window, state.winrt_device, state.capture_plan.source_width,
      state.capture_plan.source_height, [&app_state]() { on_frame_arrived(app_state); }, 3,
      capture_options);

  if (!capture_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  state.start_qpc_100ns = query_qpc_100ns();
  state.accepting_input.store(true, std::memory_order_release);

  // 从这里开始，WGC 可能随时通知 on_frame_arrived。
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  state.status.store(Features::Recording::Types::RecordingStatus::Recording,
                     std::memory_order_release);

  if (state.has_audio.load(std::memory_order_acquire)) {
    Features::Recording::AudioCapture::start_capture_thread(app_state);
  }

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

auto stop(Core::State::AppState& app_state) -> void {
  if (!app_state.recording) {
    return;
  }

  auto& state = *app_state.recording;

  auto expected = Features::Recording::Types::RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::Recording::Types::RecordingStatus::Stopping,
                                            std::memory_order_acq_rel)) {
    return;
  }

  // 先关输入入口。已经进入队列的数据会继续写完，新来的帧/音频会被拒绝。
  state.accepting_input.store(false, std::memory_order_release);

  // 先停止 WGC 继续产帧，但保留 frame pool，让编码线程排空已经到达的帧。
  auto stop_capture_start = std::chrono::steady_clock::now();
  Features::Recording::Session::stop_capture_input(app_state);
  Logger().debug("Recording capture stopped in {}ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - stop_capture_start)
                     .count());

  if (state.has_audio.load(std::memory_order_acquire)) {
    auto stop_audio_start = std::chrono::steady_clock::now();
    Features::Recording::AudioCapture::stop(state.audio);
    Logger().debug("Recording audio stopped in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - stop_audio_start)
                       .count());
  }

  // 通知编码线程收尾。它会把队列里剩下的数据写完，然后 finalize。
  Features::Recording::EncoderLoop::signal_encoder_finish(app_state);

  // 不 detach。stop 等编码线程自然结束，这样 MF/SinkWriter 的生命周期是明确的。
  if (state.encoder_thread.joinable()) {
    state.encoder_thread.join();
  }

  Features::Recording::AudioCapture::cleanup(state.audio);
  Features::Recording::Session::cleanup_capture_session(app_state);

  // finalize 成功才把无扩展名临时文件改成 .mp4；否则删除临时文件。
  if (state.finalize_succeeded) {
    auto rename_result = Features::Recording::Session::rename_working_output_to_final(
        state.working_output_path, state.config.output_path);
    if (!rename_result) {
      Logger().error("Failed to publish finalized recording '{}': {}",
                     state.config.output_path.string(), rename_result.error());
    }
  } else {
    auto reason = state.encoder_error.empty() ? "too few video frames or finalize skipped"
                                              : state.encoder_error;
    Features::Recording::Session::delete_working_output_file(state.working_output_path, reason);
  }

  auto dropped_audio = state.dropped_audio_packets.load(std::memory_order_relaxed);
  if (dropped_audio > 0) {
    Logger().warn("Recording queue dropped audio packets: {}", dropped_audio);
  }
  Features::Recording::Session::clear_session_runtime_fields(app_state);
  state.status.store(Features::Recording::Types::RecordingStatus::Idle, std::memory_order_release);
  Features::Recording::Session::start_cleanup_timer(app_state, [&app_state]() {
    request_control_action(app_state, Types::RecordingControlAction::CleanupD3D);
  });
  Logger().info("Recording stopped");
}

auto cleanup(Core::State::AppState& app_state) -> void {
  stop(app_state);
  if (!app_state.recording) {
    MFShutdown();
    return;
  }

  Features::Recording::Session::cleanup_d3d_resources(app_state);
  MFShutdown();
}

}  // namespace Features::Recording
