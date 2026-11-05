#include "features/recording/usecase.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/mfapi.hpp"

#include "core/events/events.hpp"
#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/notifications/types.hpp"
#include "core/state/app_state.hpp"
#include "features/adb_mode/device_session.hpp"
#include "features/adb_mode/usecase.hpp"
#include "features/gallery/gallery.hpp"
#include "features/gallery/watcher/watcher.hpp"
#include "features/recording/recording.hpp"
#include "features/recording/state.hpp"
#include "features/recording/types.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/floating_window/events.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "utils/graphics/hdr.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/audio_capture.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"
#include "utils/system/system.hpp"

namespace features::recording {

namespace {

// 根据本地时间戳格式化生成录制分片文件的完整存储路径
auto build_output_path_in_directory(const std::filesystem::path& directory)
    -> std::filesystem::path {
  const auto filename = std::format("{}.mp4", utils::string::FormatLocalTimestamp());
  return directory / filename;
}

// 响应通知栏查看动作：根据用户设置在资源管理器定位文件或用默认应用打开
auto handle_saved_file_view_action(core::AppState& state, const std::filesystem::path& path,
                                   std::string_view file_kind) -> void {
  const auto& action = state.settings->raw.features.saved_file_view_action;
  auto action_result = action == "reveal_in_explorer"
                           ? utils::system::reveal_file_in_explorer(path)
                           : utils::system::open_file_with_default_app(path);
  if (!action_result) {
    Logger().warn("Failed to handle {} view action '{}': {}", file_kind, action,
                  action_result.error());
  }
}

// 弹出通用录制消息通知浮条
auto notify_message(core::AppState& state, const std::string& message) -> void {
  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = utils::string::FromUtf8(message);
  core::notifications::post_notification_request(state, std::move(options));
}

// 弹出录制正在停止通知
auto notify_stopping(core::AppState& state) -> void {
  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = utils::string::FromUtf8(state.i18n->texts["message.recording_stopping"]);
  options.duration = std::chrono::milliseconds(1500);
  core::notifications::post_notification_request(state, std::move(options));
}

// 线程安全地提取并重置挂起的录制启动请求
auto take_pending_start_request(RecordingState& state)
    -> std::optional<features::recording::StartRequest> {
  std::lock_guard request_lock(state.control_request_mutex);
  if (!state.pending_start_request) {
    return std::nullopt;
  }

  auto request = std::move(state.pending_start_request);
  state.pending_start_request.reset();
  return request;
}

// 根据桌面录制停止结果分发通知，区分正常保存、短段丢弃与封装失败
auto show_recording_stop_result_notification(core::AppState& state,
                                             const features::recording::StopResult& stop_result)
    -> void {
  using features::recording::StopResultKind;

  switch (stop_result.kind) {
    case StopResultKind::Saved:
      notify_recording_saved(state, stop_result.output_path);
      return;

    case StopResultKind::Discarded: {
      auto detail = stop_result.error.empty()
                        ? utils::string::ToUtf8(stop_result.output_path.wstring())
                        : stop_result.error;
      notify_message(state, state.i18n->texts["message.recording_failed"] + detail);
      return;
    }

    case StopResultKind::Failed: {
      auto detail = utils::string::ToUtf8(stop_result.output_path.wstring());
      if (!stop_result.error.empty()) {
        detail += "\n" + stop_result.error;
      }
      notify_message(state, state.i18n->texts["message.recording_stop_failed"] + detail);
      return;
    }

    case StopResultKind::NotRecording:
    default:
      return;
  }
}

// 结束录制输出：将新生成的视频文件完整登记并加入画廊索引
auto finish_recording_output(core::AppState& state, const std::filesystem::path& path) -> void {
  auto result = features::gallery::complete_created_file_operation(state, path);
  if (!result) {
    Logger().warn("Failed to complete recording gallery operation '{}': {}", path.string(),
                  result.error());
  }
}

// 校验输出路径 → 挂载画廊监控白名单 → 启动分段录制流水线
auto start_desktop_segment(core::AppState& state, HWND target_window, const RecordingConfig& config)
    -> std::expected<void, std::string> {
  // 启动失败后的入库只能处理本次创建的文件，不能误接管已有文件
  std::error_code ec;
  if (std::filesystem::exists(config.output_path, ec)) {
    return std::unexpected("Recording output already exists: " + config.output_path.string());
  }
  if (ec) {
    return std::unexpected("Failed to inspect recording output: " + ec.message());
  }

  // 通知文件系统监听器忽略正在写入的录制临时文件
  auto ignored = features::gallery::watcher::begin_manual_file_system_ignore(
      state, config.output_path, config.output_path);
  if (!ignored) {
    return std::unexpected(ignored.error());
  }

  // 启动底层录制会话
  auto started = features::recording::start(state, target_window, config);
  if (!started) {
    // 引擎返回失败前已关闭 writer 并处理短段，保留的文件仍要尝试入库
    finish_recording_output(state, config.output_path);
  }
  return started;
}

// 停止桌面录制段并完成产物画廊入库
auto stop_desktop_segment(core::AppState& state) -> StopResult {
  auto result = features::recording::stop(state);
  if (result.kind == StopResultKind::NotRecording) {
    return result;
  }

  finish_recording_output(state, result.output_path);
  return result;
}

// 窗口尺寸变更切段：停止旧编码器分段 → 生成新时间戳路径 → 启动新尺寸分段
auto restart_after_resize(core::AppState& app_state) -> void {
  auto& state = *app_state.recording;

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  if (state.status.load(std::memory_order_acquire) !=
      features::recording::RecordingStatus::Recording) {
    return;
  }

  if (!state.session) {
    return;
  }

  auto restart_config = state.session->config;
  auto target_window = state.session->target_window;

  // 尺寸变了 → 旧编码器不能继续写入，先保存当前段，再用新尺寸开新段
  restart_config.output_path =
      build_output_path_in_directory(restart_config.output_path.parent_path());
  Logger().info("Recording restarted with timestamp output after resize: {}",
                restart_config.output_path.string());

  const auto stopped = stop_desktop_segment(app_state);
  if (stopped.kind == StopResultKind::Failed) {
    show_recording_stop_result_notification(app_state, stopped);
    core::events::post(app_state,
                       ui::floating_window::events::RecordingToggleEvent{.enabled = false});
    return;
  }

  if (!target_window || !IsWindow(target_window)) {
    show_recording_stop_result_notification(app_state, stopped);
    core::events::post(app_state,
                       ui::floating_window::events::RecordingToggleEvent{.enabled = false});
    return;
  }

  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  // 检查在停止旧段期间是否提交了用户主动停止动作
  bool cancelled = false;
  {
    std::lock_guard lock(state.control_request_mutex);
    cancelled = state.pending_action == RecordingControlAction::UserStop ||
                state.pending_action == RecordingControlAction::AbortWithError ||
                state.pending_action == RecordingControlAction::ShutdownStop;
  }
  if (cancelled) {
    show_recording_stop_result_notification(app_state, stopped);
    core::events::post(app_state,
                       ui::floating_window::events::RecordingToggleEvent{.enabled = false});
    return;
  }

  // 以新尺寸重新拉起下一段录制
  auto restart_result = start_desktop_segment(app_state, target_window, restart_config);
  if (!restart_result) {
    state.status.store(RecordingStatus::Idle);
    notify_message(app_state, app_state.i18n->texts["message.recording_start_failed"] +
                                  restart_result.error());
    core::events::post(app_state,
                       ui::floating_window::events::RecordingToggleEvent{.enabled = false});
    Logger().error("Failed to restart recording after resize: {}", restart_result.error());
  } else {
    ui::floating_window::request_repaint(app_state);
  }
}

// 控制线程串行执行录制启停、尺寸切段和退出收尾
auto handle_control_action(core::AppState& app_state,
                           features::recording::RecordingControlAction action) -> bool {
  auto& state = *app_state.recording;
  switch (action) {
    case RecordingControlAction::UserStart: {
      // 取出用户提交的启动请求参数
      auto start_request = take_pending_start_request(state);
      if (!start_request) {
        state.status.store(features::recording::RecordingStatus::Idle, std::memory_order_release);
        ui::floating_window::request_repaint(app_state);
        Logger().warn("Recording start request is missing");
        return true;
      }

      auto start_result =
          start_desktop_segment(app_state, start_request->target_window, start_request->config);
      if (!start_result) {
        // 启动失败时回退状态并提示用户
        state.status.store(features::recording::RecordingStatus::Idle, std::memory_order_release);
        ui::floating_window::request_repaint(app_state);
        notify_message(app_state, app_state.i18n->texts["message.recording_start_failed"] +
                                      start_result.error());
      } else {
        // 启动成功后通知浮窗切换录制态
        notify_message(app_state, app_state.i18n->texts["message.recording_started"]);
        core::events::post(app_state,
                           ui::floating_window::events::RecordingToggleEvent{.enabled = true});
      }
      return true;
    }

    case RecordingControlAction::UserStop:
    case RecordingControlAction::AbortWithError: {
      auto stop_result = stop_desktop_segment(app_state);
      if (stop_result.kind != StopResultKind::NotRecording) {
        show_recording_stop_result_notification(app_state, stop_result);
        core::events::post(app_state,
                           ui::floating_window::events::RecordingToggleEvent{.enabled = false});
      }
      return true;
    }

    case RecordingControlAction::RestartAfterResize:
      // 窗口尺寸变了，停当前段、用新尺寸开新段
      restart_after_resize(app_state);
      return true;

    case RecordingControlAction::ShutdownStop:
      (void)stop_desktop_segment(app_state);
      return false;

    default:
      return true;
  }
}

// 控制线程主循环：初始化 COM、监听控制动作请求并保证执行期间不阻塞 UI 线程
auto control_thread_proc(core::AppState& app_state, std::stop_token stop_token) -> void {
  auto& state = *app_state.recording;
  try {
    auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // stop_token 变化时唤醒控制线程，避免 join/shutdown 时长时间睡眠
    std::stop_callback wake_on_stop(stop_token, [&state]() { state.control_cv.notify_all(); });

    while (true) {
      RecordingControlAction action{RecordingControlAction::None};

      {
        std::unique_lock request_lock(state.control_request_mutex);
        // 平时睡在这里，有请求或 stop_token 时醒来
        state.control_cv.wait(request_lock, [&]() {
          return stop_token.stop_requested() ||
                 state.pending_action != RecordingControlAction::None;
        });

        // 只是被 stop_token 唤醒但没有 pending 请求 → 退出
        if (stop_token.stop_requested() && state.pending_action == RecordingControlAction::None) {
          break;
        }

        // 取出请求后立刻释放锁，这样 UI 线程可以继续提交新请求而不被 start/stop 阻塞
        action = state.pending_action;
        state.pending_action = RecordingControlAction::None;
        state.control_action_running.store(true, std::memory_order_release);
      }

      // 执行请求时已经不拿锁了
      bool keep_running = true;
      try {
        keep_running = handle_control_action(app_state, action);
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

// 懒启动控制线程；后续录制复用同一个后台工作线程
auto ensure_control_thread_started(core::AppState& app_state) -> std::expected<void, std::string> {
  auto& state = *app_state.recording;

  // shutdown 中不允许启动新控制线程
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  // 线程已启动则直接返回
  if (state.control_thread.joinable()) {
    return {};
  }

  state.control_thread = std::jthread(
      [&app_state](std::stop_token stop_token) { control_thread_proc(app_state, stop_token); });
  return {};
}

// 等待控制线程退出（shutdown 时调用）
auto join_control_thread(core::AppState& app_state) -> void {
  auto& state = *app_state.recording;

  if (state.control_thread.joinable()) {
    state.control_thread.join();
  }
}

}  // namespace

// 发送编码过载通知
auto notify_recording_overload(core::AppState& state) -> void {
  notify_message(state, state.i18n->texts["message.recording_overload"]);
}

// 发送文件写入积压通知，不中断录制。
auto notify_recording_write_backlog(core::AppState& state) -> void {
  notify_message(state, state.i18n->texts["message.recording_write_backlog"]);
}

// 发送录制完成保存的通知及资源管理器查看动作
auto notify_recording_saved(core::AppState& state, const std::filesystem::path& saved_path)
    -> void {
  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message =
      utils::string::FromUtf8(state.i18n->texts["message.recording_saved"]) + saved_path.wstring();

  core::notifications::NotificationAction view_action;
  view_action.label = utils::string::FromUtf8(state.i18n->texts["notification.action.view"]);
  view_action.callback = [saved_path](core::AppState& app_state) {
    handle_saved_file_view_action(app_state, saved_path, "recording");
  };
  options.action = std::move(view_action);

  core::notifications::post_notification_request(state, std::move(options));
}

// 响应用户开关快捷键/按钮：校验目标窗口与配置 → 提交 UserStart 或 UserStop 控制动作
auto toggle_recording(core::AppState& state) -> std::expected<void, std::string> {
  if (!state.recording) {
    return std::unexpected("Recording state is not initialized");
  }

  // shutdown 已经开始时，不再接受新的录制开关请求，避免退出阶段和 toggle 抢状态
  if (state.recording->shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Recording shutdown is in progress");
  }

  // 停止路由由活动会话决定，即使设备已经断开也必须交回 ADB 队列收尾
  if (features::adb_mode::is_recording_active(state)) {
    if (!features::adb_mode::toggle_recording_async(state)) {
      return std::unexpected("Failed to queue ADB recording request");
    }
    return {};
  }

  // --- 以下是 Windows 本地窗口录制工作流 ---
  // 当前正在录制 → 按一下就是停止
  auto status = state.recording->status.load(std::memory_order_acquire);
  if (status == features::recording::RecordingStatus::Recording) {
    if (!features::recording::enter_stopping(state)) {
      return std::unexpected("Failed to enter recording stopping state");
    }
    // 截止点已冻结，不能回滚为Recording；若被Abort/Shutdown合并，它们也会完成停止
    (void)features::recording::request_control_action(state, RecordingControlAction::UserStop);
    return {};
  }

  // 正在停止时提示封装进度；再次停止也可取消resize后的自动重启
  if (status == features::recording::RecordingStatus::Stopping) {
    features::recording::request_control_action(state, RecordingControlAction::UserStop);
    notify_stopping(state);
    return {};
  }

  // 不是 Idle 的其他瞬态状态（例如 Starting）也直接忽略，避免 toggle 抢状态机
  if (status != features::recording::RecordingStatus::Idle) {
    return {};
  }

  // 切段/入库期间可能短暂Idle，但控制线程仍拥有当前操作，不能插入第二次启动
  if (state.recording->control_action_running.load(std::memory_order_acquire)) {
    return {};
  }

  // 没有桌面活动段时才按连接状态选择新录制的后端
  if (features::adb_mode::session::is_connected(state)) {
    if (!features::adb_mode::toggle_recording_async(state)) {
      return std::unexpected("Failed to queue ADB recording request");
    }
    return {};
  }

  // --- 以下是启动流程 ---

  // 先找目标窗口
  std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
  auto target = features::window_control::find_target_window(window_title);
  if (!target) {
    notify_message(state, state.i18n->texts["message.window_not_found"]);
    return std::unexpected("Target window not found");
  }

  // 输出目录在录制开始时确定，后续自动切段沿用同一个窗口目录
  auto output_dir_result =
      utils::path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
  if (state.settings->raw.features.organize_output_by_window_title) {
    auto actual_title = features::window_control::get_window_title(*target);
    if (!actual_title) {
      Logger().warn("Failed to read current recording target title, using configured title: {}",
                    actual_title.error());
    }
    output_dir_result = utils::path::GetOutputDirectoryForWindowTitle(
        state.settings->raw.features.output_dir_path, actual_title.value_or(window_title));
  }
  if (!output_dir_result) {
    notify_message(state,
                   state.i18n->texts["message.recording_start_failed"] + output_dir_result.error());
    return std::unexpected("Failed to get output directory: " + output_dir_result.error());
  }

  // 从用户设置组装录制配置（码率、帧率、编码器、HDR 等）
  const auto& recording_settings = state.settings->raw.features.recording;
  features::recording::RecordingConfig config;
  config.output_path = build_output_path_in_directory(*output_dir_result);
  config.fps = recording_settings.fps;
  config.bitrate = recording_settings.bitrate;
  config.quality = recording_settings.quality;
  config.qp = recording_settings.qp;
  config.rate_control =
      features::recording::rate_control_mode_from_string(recording_settings.rate_control);
  config.encoder_mode =
      features::recording::encoder_mode_from_string(recording_settings.encoder_mode);
  config.codec = features::recording::video_codec_from_string(recording_settings.codec);
  config.enable_hdr = recording_settings.enable_hdr;
  config.capture_client_area = recording_settings.capture_client_area;
  config.capture_cursor = recording_settings.capture_cursor;
  config.auto_restart_on_resize = recording_settings.auto_restart_on_resize;
  config.audio_source =
      utils::media::audio_capture::audio_source_from_string(recording_settings.audio_source);
  config.audio_bitrate = recording_settings.audio_bitrate;

  // HDR 录制：先查显示器是否真的启用了 HDR，如果不是就降级为 SDR
  if (recording_settings.enable_hdr) {
    config.enable_hdr = false;
    auto hdr_info = utils::graphics::hdr::query_monitor_hdr_info(*target);
    if (hdr_info) {
      config.enable_hdr = hdr_info->hdr_active;
      if (config.enable_hdr) {
        config.hdr_target_peak_nits = static_cast<std::uint32_t>(
            std::lround(std::clamp(hdr_info->max_luminance_nits, 203.0f, 10000.0f)));
      }
    } else {
      Logger().warn("Failed to query HDR monitor info for recording: {}", hdr_info.error());
    }
  }

  // HDR 模式下必须用 H.265 + GPU 编码器，否则报错
  if (config.enable_hdr) {
    if (config.codec != features::recording::VideoCodec::H265) {
      std::string error = "HDR recording requires H.265 codec";
      notify_message(state, state.i18n->texts["message.recording_start_failed"] + error);
      return std::unexpected(error);
    }
    if (config.encoder_mode == features::recording::EncoderMode::CPU) {
      std::string error = "HDR recording requires GPU encoder";
      notify_message(state, state.i18n->texts["message.recording_start_failed"] + error);
      return std::unexpected(error);
    }
  }

  // 首次录制时懒启动控制线程；后续录制复用同一个线程
  if (auto result = ensure_control_thread_started(state); !result) {
    return result;
  }

  // CAS 将状态从 Idle 原子切换为 Starting，防止并发 toggle 重复启动
  auto expected_status = features::recording::RecordingStatus::Idle;
  if (!state.recording->status.compare_exchange_strong(
          expected_status, features::recording::RecordingStatus::Starting,
          std::memory_order_acq_rel)) {
    return std::unexpected("Recording is not idle");
  }

  // 将目标窗口与组装好的配置挂起到控制请求槽，供控制线程取出执行
  {
    std::lock_guard request_lock(state.recording->control_request_mutex);
    state.recording->pending_start_request =
        features::recording::StartRequest{.target_window = *target, .config = config};
  }

  // 投递 UserStart 动作；入队失败时回滚状态与挂起请求
  if (!features::recording::request_control_action(
          state, features::recording::RecordingControlAction::UserStart)) {
    std::lock_guard request_lock(state.recording->control_request_mutex);
    state.recording->pending_start_request.reset();
    state.recording->status.store(features::recording::RecordingStatus::Idle,
                                  std::memory_order_release);
    std::string error = "Failed to queue recording start request";
    notify_message(state, state.i18n->texts["message.recording_start_failed"] + error);
    return std::unexpected(error);
  }

  return {};
}

// 应用退出或停机时停止进行中的录制会话并等待后台工作线程退出
auto stop_recording_if_running(core::AppState& state) -> void {
  // 先拒绝新的 toggle；ADB 由最终队列任务收尾录制并关闭设备，只等待一次线程退出
  if (state.recording) {
    state.recording->shutdown_requested.store(true, std::memory_order_release);
    if (state.recording->control_thread.joinable()) {
      features::recording::request_control_action(state, RecordingControlAction::ShutdownStop);
    }
  }

  if (state.adb_mode) {
    features::adb_mode::shutdown(state);
  }

  if (!state.recording) {
    return;
  }

  if (state.recording->control_thread.joinable()) {
    // ShutdownStop 已先投递，等待控制线程完成收尾
    join_control_thread(state);
  } else if (state.recording->status.load(std::memory_order_acquire) ==
             features::recording::RecordingStatus::Recording) {
    // 控制线程是懒启动的，没有消费者时它不存在；直接兜底停止
    Logger().warn(
        "Recording is active but control thread is not running during shutdown; stopping directly");
    (void)stop_desktop_segment(state);
  }
}

// 清理录制子系统资源并关闭 Media Foundation 运行时
auto cleanup(core::AppState& state) -> void {
  stop_recording_if_running(state);
  MFShutdown();
}

}  // namespace features::recording
