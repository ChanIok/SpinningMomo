module;

module Features.Recording;

import std;
import Core.Events;
import Core.I18n.State;
import Core.Notifications;
import Core.Notifications.Types;
import Core.State;
import Features.Recording.EncoderLoop;
import Features.Recording.Session;
import Features.Recording.State;
import Features.Recording.Time;
import Features.Recording.Types;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import Utils.Graphics.Capture;
import Utils.Logger;
import Utils.Media.AudioCapture;
import Utils.String;
import Utils.System;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording {

// 录制模块的大致数据流：
// WGC 帧回调只负责唤醒编码线程；音频采集线程只复制 PCM 数据并入队；
// 编码线程按固定 fps 消费捕获（每次最多取一帧 Copy 到自有纹理），是唯一写 SinkWriter 的地方；
// 控制线程负责把 start / stop / resize restart 串起来，避免多个重操作互相打架。
auto start(Core::State::AppState& app_state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;
auto stop(Core::State::AppState& app_state) -> Features::Recording::Types::StopResult;
auto request_control_action(Core::State::AppState& app_state,
                            Features::Recording::Types::RecordingControlAction action) -> bool;

auto show_recording_notification(Core::State::AppState& state, const std::string& message) -> void {
  Core::Notifications::Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = Utils::String::FromUtf8(message);
  Core::Notifications::post_notification_request(state, std::move(options));
}

auto show_recording_saved_notification(Core::State::AppState& state,
                                       const std::filesystem::path& saved_path) -> void {
  Core::Notifications::Types::NotificationOptions options;
  options.title = Utils::String::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message =
      Utils::String::FromUtf8(state.i18n->texts["message.recording_saved"]) + saved_path.wstring();

  Core::Notifications::Types::NotificationAction view_action;
  view_action.label = Utils::String::FromUtf8(state.i18n->texts["notification.action.view"]);
  view_action.callback = [saved_path](Core::State::AppState&) {
    auto open_result = Utils::System::open_file_with_default_app(saved_path);
    if (!open_result) {
      Logger().warn("Failed to open recording: {}", open_result.error());
    }
  };
  options.action = std::move(view_action);

  Core::Notifications::post_notification_request(state, std::move(options));
}

auto show_recording_stop_result_notification(
    Core::State::AppState& state, const Features::Recording::Types::StopResult& stop_result)
    -> void {
  using Features::Recording::Types::StopResultKind;

  switch (stop_result.kind) {
    case StopResultKind::Saved:
      show_recording_saved_notification(state, stop_result.output_path);
      return;

    case StopResultKind::Discarded: {
      auto detail = stop_result.error.empty()
                        ? Utils::String::ToUtf8(stop_result.output_path.wstring())
                        : stop_result.error;
      show_recording_notification(state, state.i18n->texts["message.recording_failed"] + detail);
      return;
    }

    case StopResultKind::PublishFailed: {
      auto detail = stop_result.error.empty()
                        ? Utils::String::ToUtf8(stop_result.output_path.wstring())
                        : stop_result.error;
      show_recording_notification(state,
                                  state.i18n->texts["message.recording_stop_failed"] + detail);
      return;
    }

    case StopResultKind::NotRecording:
    default:
      return;
  }
}

// 启动 WASAPI 音频采集线程：PCM 数据入队后唤醒编码线程消费
auto start_audio_capture_thread(Core::State::AppState& app_state) -> void {
  auto& state = *app_state.recording;

  // 启动 WASAPI 采集线程，回调会在音频数据到达时被调用
  Utils::Media::AudioCapture::start_capture_thread(
      state.audio, [&state](const BYTE* data, UINT32 num_frames, UINT32 bytes_per_frame,
                            UINT64 qpc_position_100ns, DWORD flags) {
        // stop 后拒绝继续入队
        if (num_frames == 0 || bytes_per_frame == 0 ||
            !state.accepting_input.load(std::memory_order_acquire) ||
            state.finish_requested.load(std::memory_order_acquire) ||
            !state.has_audio.load(std::memory_order_acquire)) {
          return;
        }

        // 把 PCM 数据封装成 QueuedAudioPacket，换算时间戳到录制时间线
        Types::QueuedAudioPacket packet;
        packet.num_frames = num_frames;
        packet.bytes_per_frame = bytes_per_frame;
        const bool timestamp_valid =
            qpc_position_100ns > 0 && !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR);
        const auto absolute_qpc_100ns = timestamp_valid
                                            ? static_cast<std::int64_t>(qpc_position_100ns)
                                            : Features::Recording::Time::query_qpc_100ns();
        packet.timestamp_100ns = Features::Recording::Time::relative_timestamp_100ns(
            state.start_qpc_100ns, absolute_qpc_100ns);
        if (state.audio.wave_format) {
          packet.sample_rate = state.audio.wave_format->nSamplesPerSec;
        }

        // 拷贝 PCM 数据（静音包不需要拷贝）
        const auto byte_count = static_cast<std::size_t>(num_frames) * bytes_per_frame;
        packet.data.resize(byte_count);
        if (data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
          std::memcpy(packet.data.data(), data, byte_count);
        }

        // 推入队列，队列满时丢弃最老的包
        {
          std::lock_guard queue_lock(state.queue_mutex);
          if (!state.accepting_input.load(std::memory_order_acquire)) {
            return;
          }
          if (state.audio_queue.size() >= Types::k_max_audio_queue_size) {
            state.audio_queue.pop_front();
            state.dropped_audio_packets.fetch_add(1, std::memory_order_relaxed);
          }
          state.audio_queue.push_back(std::move(packet));
        }
        // 唤醒编码线程消费音频
        state.queue_cv.notify_one();
      });
}

// 请求优先级从高到低：ShutdownStop > AbortWithError > RestartAfterResize > CleanupD3D
auto request_control_action(Core::State::AppState& app_state,
                            Features::Recording::Types::RecordingControlAction action) -> bool {
  auto& state = *app_state.recording;

  // shutdown 后只接受 ShutdownStop，其他请求一律拒绝
  if (state.shutdown_requested.load(std::memory_order_acquire) &&
      action != Types::RecordingControlAction::ShutdownStop) {
    return false;
  }

  {
    std::lock_guard request_lock(state.control_request_mutex);

    // shutdown 优先级最高，直接覆盖一切 pending 请求
    if (action == Types::RecordingControlAction::ShutdownStop) {
      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // AbortWithError 不能被 shutdown 以外的请求覆盖
    if (action == Types::RecordingControlAction::AbortWithError) {
      if (state.pending_action == Types::RecordingControlAction::ShutdownStop) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // resize restart 可以被后来的 resize 覆盖，但不能插到 shutdown 或 abort 前面
    if (action == Types::RecordingControlAction::RestartAfterResize) {
      if (state.pending_action == Types::RecordingControlAction::ShutdownStop ||
          state.pending_action == Types::RecordingControlAction::AbortWithError) {
        return false;
      }

      state.pending_action = action;
      state.control_cv.notify_one();
      return true;
    }

    // CleanupD3D 优先级最低，前面有任一请求时都跳过
    if (action == Types::RecordingControlAction::CleanupD3D) {
      if (state.pending_action == Types::RecordingControlAction::ShutdownStop ||
          state.pending_action == Types::RecordingControlAction::AbortWithError ||
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

// 处理单个控制动作。返回 false 时控制线程退出循环。
auto handle_control_action(Core::State::AppState& app_state,
                           Features::Recording::State::RecordingState& state,
                           Features::Recording::Types::RecordingControlAction action) -> bool {
  switch (action) {
    case Types::RecordingControlAction::AbortWithError:
      // 编码器出错时紧急停止并通知用户
      {
        auto stop_result = Features::Recording::stop(app_state);
        if (stop_result.kind != Features::Recording::Types::StopResultKind::NotRecording) {
          show_recording_stop_result_notification(app_state, stop_result);
          Core::Events::post(app_state,
                             UI::FloatingWindow::Events::RecordingToggleEvent{.enabled = false});
        }
      }
      return true;

    case Types::RecordingControlAction::RestartAfterResize:
      // 窗口尺寸变了，停当前段、用新尺寸开新段
      restart_after_resize(app_state);
      return true;

    case Types::RecordingControlAction::ShutdownStop:
      // 应用退出，停止录制并退出控制线程
      if (state.status.load(std::memory_order_acquire) ==
          Features::Recording::Types::RecordingStatus::Recording) {
        (void)Features::Recording::stop(app_state);
      }
      return false;

    case Types::RecordingControlAction::CleanupD3D:
      // 空闲 5 秒后释放 D3D 设备，减少显存占用
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

// 控制线程主循环：睡在 condition_variable 上，收到请求就串行执行 start/stop/restart
auto control_thread_proc(Core::State::AppState& app_state,
                         Features::Recording::State::RecordingState& state,
                         std::stop_token stop_token) -> void {
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // stop_token 被 request_stop 时会唤醒控制线程
    std::stop_callback wake_on_stop(stop_token, [&state]() { state.control_cv.notify_all(); });

    while (true) {
      Types::RecordingControlAction action{Types::RecordingControlAction::None};

      {
        std::unique_lock request_lock(state.control_request_mutex);
        // 平时睡在这里，有请求或 stop_token 时醒来
        state.control_cv.wait(request_lock, [&]() {
          return stop_token.stop_requested() ||
                 state.pending_action != Types::RecordingControlAction::None;
        });

        // 只是被 stop_token 唤醒但没有 pending 请求 → 退出
        if (stop_token.stop_requested() &&
            state.pending_action == Types::RecordingControlAction::None) {
          break;
        }

        // 取出请求后立刻释放锁，这样 UI 线程可以继续提交新请求而不被 start/stop 阻塞
        action = state.pending_action;
        state.pending_action = Types::RecordingControlAction::None;
        state.control_action_running.store(true, std::memory_order_release);
      }

      // 执行请求时已经不拿锁了
      bool keep_running = true;
      try {
        keep_running = handle_control_action(app_state, state, action);
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

// 懒启动控制线程，首次录制时创建，后续复用
auto ensure_control_thread_started(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  auto& state = *app_state.recording;

  // shutdown 中不允许启动新控制线程
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  // 线程已启动则直接返回
  if (state.control_thread.joinable()) {
    return {};
  }

  state.control_thread = std::jthread([&app_state](std::stop_token stop_token) {
    auto& rec = *app_state.recording;
    control_thread_proc(app_state, rec, stop_token);
  });
  return {};
}

auto join_control_thread(Core::State::AppState& app_state) -> void {
  // 等待控制线程退出（shutdown 时调用）
  auto& state = *app_state.recording;

  if (state.control_thread.joinable()) {
    state.control_thread.join();
  }
}

auto request_restart_after_resize(Core::State::AppState& app_state) -> void {
  // WGC 帧尺寸变化时由编码线程回调触发
  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  request_control_action(app_state, Types::RecordingControlAction::RestartAfterResize);
}

// 窗口尺寸变化时停止当前段，用新尺寸启动新段，对用户无感
auto restart_after_resize(Core::State::AppState& app_state) -> void {
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

  // 尺寸变了 → 老编码器不能再继续写入，先保存当前段，再用新尺寸开新段
  restart_config.output_path = Features::Recording::Session::build_output_path_in_directory(
      restart_config.output_path.parent_path());
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
  // 初始化 Media Foundation 运行时（在应用启动时调用一次）
  (void)app_state;
  if (FAILED(MFStartup(MF_VERSION))) {
    return std::unexpected("Failed to initialize Media Foundation");
  }
  return {};
}

auto on_frame_arrived(Core::State::AppState& app_state) -> void {
  // WGC 帧回调的简单转发：标记 pending 并唤醒编码线程
  Features::Recording::EncoderLoop::mark_video_frame_pending(app_state);
}

// start 中途失败时的统一清理：释放已分配的资源，保持和 stop 一致的顺序
auto cleanup_failed_start(Core::State::AppState& app_state, std::string_view reason) -> void {
  auto& state = *app_state.recording;

  // start 中途失败也走一遍停输入 → 停止产帧 → 停编码线程 → 清资源的顺序，
  // 和正常 stop 的资源释放路径保持一致
  state.accepting_input.store(false, std::memory_order_release);
  Features::Recording::Session::stop_capture_input(app_state);
  Utils::Media::AudioCapture::stop(state.audio);
  Features::Recording::EncoderLoop::signal_encoder_finish(app_state);

  if (state.encoder_thread.joinable()) {
    state.encoder_thread.request_stop();
    state.encoder_thread.join();
  }

  Utils::Media::AudioCapture::cleanup(state.audio);
  Features::Recording::Session::cleanup_capture_session(app_state);
  Features::Recording::Session::delete_working_output_file(state.working_output_path, reason);
  Features::Recording::Session::clear_session_runtime_fields(app_state);
}

// 启动一个录制段：算尺寸 → 起 D3D → 起音频 → 起编码线程 → 起 WGC 捕获
auto start(Core::State::AppState& app_state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string> {
  if (!app_state.recording) {
    return std::unexpected("RecordingState is not initialized");
  }

  auto& state = *app_state.recording;

  // 只接受 Idle 状态，避免重复 start
  auto current_status = state.status.load(std::memory_order_acquire);
  if (current_status != Features::Recording::Types::RecordingStatus::Idle) {
    return std::unexpected("Recording is not idle");
  }

  if (IsIconic(target_window)) {
    return std::unexpected("Target window is minimized");
  }

  // 清空上一次录制残留的会话状态，取消 D3D 延迟释放定时器
  Features::Recording::Session::clear_session_runtime_fields(app_state);
  Features::Recording::Session::cancel_cleanup_timer(app_state);

  // 先算清楚这次录制的源尺寸和输出尺寸——编码器一旦创建，宽高就不能再改
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
  // 按本段 fps 算出帧间隔（100ns 单位）。默认 30fps ≈ 333333 100ns
  const auto fps = std::max<std::uint32_t>(state.config.fps, 1);
  state.video_frame_interval_100ns = std::max<std::int64_t>(1, 10'000'000LL / fps);

  // 录制间复用 D3D 设备，避免高频启停反复初始化
  auto d3d_ready_result = Features::Recording::Session::ensure_d3d_resources_ready(app_state);
  if (!d3d_ready_result) {
    Features::Recording::Session::clear_session_runtime_fields(app_state);
    return d3d_ready_result;
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(target_window, &process_id);

  // 初始化音频捕获。如果 GameOnly 模式失败，降级到 System 模式
  auto audio_source_to_use = config.audio_source;
  auto audio_result =
      Utils::Media::AudioCapture::initialize(state.audio, audio_source_to_use, process_id);
  if (!audio_result && audio_source_to_use == Utils::Media::AudioCapture::AudioSource::GameOnly) {
    Logger().warn("GameOnly audio capture initialization failed: {}, falling back to System",
                  audio_result.error());
    Utils::Media::AudioCapture::cleanup(state.audio);
    audio_source_to_use = Utils::Media::AudioCapture::AudioSource::System;
    audio_result =
        Utils::Media::AudioCapture::initialize(state.audio, audio_source_to_use, process_id);
  }

  if (!audio_result) {
    Logger().warn("Audio capture initialization failed: {}, continuing without audio",
                  audio_result.error());
  } else {
    Logger().info("Audio capture initialized");
  }

  // 先启动编码线程并等它创建好 SinkWriter，再开始 WGC 捕获。
  // 反过来的话捕获已经在产帧，编码器却还没准备好
  state.encoder_thread = std::jthread([&app_state](std::stop_token stop_token) {
    Features::Recording::EncoderLoop::encoder_thread_proc(
        app_state, stop_token, [&app_state]() { request_restart_after_resize(app_state); });
  });
  Features::Recording::EncoderLoop::wait_encoder_ready(app_state);

  if (!state.encoder_start_succeeded) {
    // 编码器启动失败，清理已分配的资源后返回错误
    std::string error =
        state.encoder_error.empty() ? "Failed to start recording encoder" : state.encoder_error;
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected(error);
  }

  Utils::Graphics::Capture::CaptureSessionOptions capture_options;
  capture_options.capture_cursor = config.capture_cursor;
  capture_options.border_required = false;
  if (fps > 60) {
    // 高帧率录制显式请求更短的 WGC 更新间隔，绕开部分系统版本默认 60Hz 节流。
    capture_options.min_update_interval = std::chrono::milliseconds(1);
  }
  if (config.enable_hdr) {
    // HDR 桌面捕获使用 scRGB 16-bit float，后续编码线程会转成 HEVC 需要的 P010
    capture_options.pixel_format =
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // WGC 的回调只打标记不拷数据，真正的取帧和编码都在编码线程里完成
  auto capture_result = Utils::Graphics::Capture::create_capture_session_with_frame_notification(
      target_window, state.winrt_device, state.capture_plan.source_width,
      state.capture_plan.source_height, [&app_state]() { on_frame_arrived(app_state); }, 3,
      capture_options);

  if (!capture_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to create capture session: " + capture_result.error());
  }
  state.capture_session = std::move(*capture_result);

  // 冻结录制起点时间戳，之后所有视频帧和音频都基于这个时间计算偏移
  state.start_qpc_100ns = Features::Recording::Time::query_qpc_100ns();
  state.accepting_input.store(true, std::memory_order_release);

  // 正式启动 WGC 捕获。从此 on_frame_arrived 可能随时被调用
  auto start_result = Utils::Graphics::Capture::start_capture(state.capture_session);
  if (!start_result) {
    cleanup_failed_start(app_state, "recording start failed");
    return std::unexpected("Failed to start capture: " + start_result.error());
  }

  state.status.store(Features::Recording::Types::RecordingStatus::Recording,
                     std::memory_order_release);

  // 如果音频设备初始化成功，启动音频采集线程
  if (state.has_audio.load(std::memory_order_acquire)) {
    start_audio_capture_thread(app_state);
  }

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

// 停止当前录制段：关输入 → 停 WGC → 停音频 → 冻结时间线 → 等编码排空 → 发布或丢弃文件
auto stop(Core::State::AppState& app_state) -> Features::Recording::Types::StopResult {
  Features::Recording::Types::StopResult result;

  if (!app_state.recording) {
    return result;
  }

  auto& state = *app_state.recording;
  result.output_path = state.config.output_path;

  // CAS 保证只有一个线程能执行到停止逻辑。如果不是 Recording 状态就跳过
  auto expected = Features::Recording::Types::RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(expected,
                                            Features::Recording::Types::RecordingStatus::Stopping,
                                            std::memory_order_acq_rel)) {
    return result;
  }

  // 第一步：关入口，已经在队列里的数据继续处理，新来的拒绝
  state.accepting_input.store(false, std::memory_order_release);

  // 第二步：停 WGC 产帧，但保留 frame pool 让编码线程排空已到达的帧
  auto stop_capture_start = std::chrono::steady_clock::now();
  Features::Recording::Session::stop_capture_input(app_state);
  Logger().debug("Recording capture stopped in {}ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - stop_capture_start)
                     .count());

  // 第三步：停音频采集
  if (state.has_audio.load(std::memory_order_acquire)) {
    auto stop_audio_start = std::chrono::steady_clock::now();
    Utils::Media::AudioCapture::stop(state.audio);
    Logger().debug("Recording audio stopped in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - stop_audio_start)
                       .count());
  }

  // 第四步：冻结 stop 的目标时间线。之后编码线程只能追这个固定目标，不能继续跟着墙钟走
  state.frozen_finish_target_100ns.store(
      Features::Recording::Time::elapsed_since_start_100ns(state.start_qpc_100ns),
      std::memory_order_release);

  // 第五步：通知编码线程收尾，等它把队列排空并 finalize 编码器
  Features::Recording::EncoderLoop::signal_encoder_finish(app_state);

  if (state.encoder_thread.joinable()) {
    Logger().debug("Waiting for recording encoder thread to finish");
    auto join_start = std::chrono::steady_clock::now();
    state.encoder_thread.join();
    Logger().debug("Recording encoder thread joined in {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - join_start)
                       .count());
  }

  // 第六步：清理音频和捕获会话
  Utils::Media::AudioCapture::cleanup(state.audio);
  Features::Recording::Session::cleanup_capture_session(app_state);

  // 第七步：finalize 成功就把无扩展名临时文件改名成 .mp4，否则删掉
  if (state.finalize_succeeded) {
    auto rename_result = Features::Recording::Session::rename_working_output_to_final(
        state.working_output_path, state.config.output_path);
    if (!rename_result) {
      Logger().error("Failed to publish finalized recording '{}': {}",
                     state.config.output_path.string(), rename_result.error());
      result.kind = Features::Recording::Types::StopResultKind::PublishFailed;
      result.error = rename_result.error();
    } else {
      result.kind = Features::Recording::Types::StopResultKind::Saved;
    }
  } else {
    auto reason = state.encoder_error.empty() ? "too few video frames or finalize skipped"
                                              : state.encoder_error;
    Features::Recording::Session::delete_working_output_file(state.working_output_path, reason);
    result.kind = Features::Recording::Types::StopResultKind::Discarded;
    result.error = reason;
  }

  auto dropped_audio = state.dropped_audio_packets.load(std::memory_order_relaxed);
  if (dropped_audio > 0) {
    Logger().warn("Recording queue dropped audio packets: {}", dropped_audio);
  }
  if (state.skipped_video_frames_due_to_encoding_lag > 0) {
    Logger().warn("Recording skipped video frames due to encoding lag: {}",
                  state.skipped_video_frames_due_to_encoding_lag);
  }

  // 第八步：清空会话态，状态改回 Idle，启动延迟 5 秒的 D3D 清理定时器
  Features::Recording::Session::clear_session_runtime_fields(app_state);
  state.status.store(Features::Recording::Types::RecordingStatus::Idle, std::memory_order_release);
  Features::Recording::Session::start_cleanup_timer(app_state, [&app_state]() {
    request_control_action(app_state, Types::RecordingControlAction::CleanupD3D);
  });
  Logger().info("Recording stopped");
  return result;
}

// 录制模块退出清理：先停录制，再关闭 Media Foundation 运行时
auto cleanup(Core::State::AppState& app_state) -> void {
  // 应用退出时：先停录制，再关闭 Media Foundation
  (void)stop(app_state);
  if (!app_state.recording) {
    MFShutdown();
    return;
  }

  Features::Recording::Session::cleanup_d3d_resources(app_state);
  MFShutdown();
}

}  // namespace Features::Recording
