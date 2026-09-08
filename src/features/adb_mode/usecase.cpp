#include "features/adb_mode/usecase.hpp"

#include "vendor/std.hpp"

#include "vendor/rfl.hpp"

#include "core/events/events.hpp"
#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/notifications/types.hpp"
#include "core/rpc/notification_hub.hpp"
#include "core/state/app_state.hpp"
#include "features/adb_mode/adb_client.hpp"
#include "features/adb_mode/device_finder.hpp"
#include "features/adb_mode/device_session.hpp"
#include "features/adb_mode/display_control.hpp"
#include "features/adb_mode/events.hpp"
#include "features/adb_mode/recording.hpp"
#include "features/adb_mode/state.hpp"
#include "features/adb_mode/types.hpp"
#include "features/settings/menu.hpp"
#include "features/settings/state.hpp"
#include "features/settings/types.hpp"
#include "features/window_control/types.hpp"
#include "ui/floating_window/state.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::adb_mode {

namespace {

using AdbTask = std::move_only_function<void()>;

constexpr auto kDisplaySettleDelay = std::chrono::milliseconds(400);

// 在设置锁保护下复制 ADB 模式配置，供后台任务独立使用。
auto get_adb_mode_settings(const core::AppState& state)
    -> features::settings::AppSettings::Features::AdbMode {
  std::scoped_lock lock(state.settings->mutation_mutex);
  return state.settings->raw.features.adb_mode;
}

// 根据当前设置构造并解析一次可直接使用的 ADB 连接配置。
auto build_connection_config(const core::AppState& state)
    -> std::expected<AdbConnectionConfig, std::string> {
  const auto settings = get_adb_mode_settings(state);
  std::filesystem::path executable;
  if (settings.use_custom_adb_path) {
    if (settings.adb_path.empty()) {
      return std::unexpected("message.adb_custom_path_empty");
    }
    executable = std::filesystem::path(utils::string::FromUtf8(settings.adb_path));
  }

  return device_finder::resolve_connection(AdbConnectionConfig{
      .executable = std::move(executable),
      .host = settings.host,
      .port = settings.port,
      .serial = settings.serial,
  });
}

// 清除当前会话的所有派生状态，调用方必须持有状态锁。
auto clear_session_locked(AdbModeState& adb_state) -> void {
  adb_state.recording_session.reset();
  adb_state.device_session.reset();
  adb_state.restore_pending = false;
}

// 更新当前操作阶段，并让状态查询立即反映队列正在处理的任务。
auto set_operation_state(core::AppState& state, ConnectionState connection_state) -> void {
  auto& adb_state = *state.adb_mode;
  std::scoped_lock lock(adb_state.mutex);
  adb_state.connection_state = connection_state;
  adb_state.operation_in_progress = true;
  adb_state.last_error.clear();
}

// 记录失败原因；连接仍可用时保留会话，确保用户还能执行恢复。
auto set_error_state(core::AppState& state, std::string error, bool retain_connection) -> void {
  auto& adb_state = *state.adb_mode;
  std::scoped_lock lock(adb_state.mutex);
  adb_state.connection_state =
      retain_connection ? ConnectionState::Connected : ConnectionState::Error;
  adb_state.last_error = core::i18n::get_text(state.i18n->texts, error);
  if (!retain_connection) {
    clear_session_locked(adb_state);
  }
}

// 读取运行时状态并通过 RPC 通知推送给 Web 前端。
auto publish_status_changed(core::AppState& state) -> void {
  try {
    const auto status = get_status(state);
    const auto params_json = rfl::json::write<rfl::SnakeCaseToCamelCase>(status);
    core::rpc::notification_hub::send_notification(state, "adbMode.changed", params_json);
  } catch (const std::exception& e) {
    Logger().warn("Failed to publish ADB mode status: {}", e.what());
  }
}

// 把一条文本消息投递为系统通知。
auto post_adb_notification(core::AppState& state, std::string message) -> void {
  core::notifications::NotificationOptions options;
  options.title = utils::string::FromUtf8(state.i18n->texts["label.app_name"]);
  options.message = utils::string::FromUtf8(message);
  core::notifications::post_notification_request(state, std::move(options));
}

// 任务无法入队时复用统一的 ADB 失败提示。
auto post_queue_failure_notification(core::AppState& state) -> void {
  post_adb_notification(state, state.i18n->texts["message.adb_operation_failed"]);
}

// 根据结果选择本地化提示，并在失败时根据错误类型友好呈现。
auto post_operation_notification(core::AppState& state,
                                 const std::expected<void, std::string>& result,
                                 std::string_view success_key, std::string_view failure_key)
    -> void {
  if (result) {
    const auto it = state.i18n->texts.find(std::string(success_key));
    post_adb_notification(state,
                          it != state.i18n->texts.end() ? it->second : "ADB operation completed");
    return;
  }

  const auto formatted_error = core::i18n::get_text(state.i18n->texts, result.error());
  const auto fallback_it = state.i18n->texts.find(std::string(failure_key));
  const auto prefix =
      fallback_it != state.i18n->texts.end() ? fallback_it->second : "ADB operation failed";
  post_adb_notification(state, prefix + ": " + formatted_error);
}

// 异步通知其他模块连接状态发生变化。
auto post_connection_changed(core::AppState& state, bool connected) -> void {
  core::events::post(state, events::ConnectionChangedEvent{.connected = connected});
}

// 把显示变换结果投递到 UI 线程，统一处理菜单同步和错误提示。
auto post_display_transform_result(core::AppState& state, std::optional<std::size_t> ratio_index,
                                   std::optional<std::size_t> resolution_index,
                                   const std::expected<void, std::string>& result) -> void {
  core::events::post(state, events::DisplayTransformCompletedEvent{
                                .ratio_index = ratio_index,
                                .resolution_index = resolution_index,
                                .success = result.has_value(),
                                .error = result ? std::string{} : result.error()});
}

// 完成一个队列任务；只有队列真正为空时才清除忙碌状态。
auto finish_task(core::AppState& state) -> void {
  auto& adb_state = *state.adb_mode;
  const auto previous_count = adb_state.pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
  if (previous_count == 0) {
    Logger().error("ADB task counter underflow");
    return;
  }
  if (previous_count != 1 || adb_state.pending_tasks.load(std::memory_order_acquire) != 0) {
    return;
  }

  {
    std::scoped_lock lock(adb_state.mutex);
    if (adb_state.pending_tasks.load(std::memory_order_acquire) == 0) {
      adb_state.operation_in_progress = false;
    }
  }
  publish_status_changed(state);
}

// ADB 专用线程按 FIFO 顺序执行所有任务，停止时仍会先处理队列中的最终恢复任务。
auto adb_thread_proc(core::AppState& state, std::stop_token stop_token) -> void {
  auto& adb_state = *state.adb_mode;
  std::stop_callback wake_on_stop(stop_token, [&adb_state]() { adb_state.queue_cv.notify_all(); });

  while (true) {
    AdbTask task;
    {
      std::unique_lock lock(adb_state.queue_mutex);
      adb_state.queue_cv.wait(
          lock, [&]() { return stop_token.stop_requested() || !adb_state.task_queue.empty(); });

      if (adb_state.task_queue.empty()) {
        if (stop_token.stop_requested()) {
          break;
        }
        continue;
      }

      task = std::move(adb_state.task_queue.front());
      adb_state.task_queue.pop();
    }

    try {
      if (task) {
        task();
      }
    } catch (const std::exception& e) {
      Logger().error("ADB worker task failed: {}", e.what());
      bool retain_connection = false;
      {
        std::scoped_lock lock(adb_state.mutex);
        retain_connection = adb_state.device_session != nullptr;
      }
      set_error_state(state, std::string("ADB worker task failed: ") + e.what(), retain_connection);
    } catch (...) {
      Logger().error("ADB worker task failed with unknown exception");
      bool retain_connection = false;
      {
        std::scoped_lock lock(adb_state.mutex);
        retain_connection = adb_state.device_session != nullptr;
      }
      set_error_state(state, "ADB worker task failed with unknown exception", retain_connection);
    }

    finish_task(state);
  }
}

// 把任务放入 ADB 专用 FIFO 队列，并在入队瞬间标记模块为忙碌。
auto enqueue_task(core::AppState& state, AdbTask task) -> bool {
  if (!task) {
    return false;
  }

  auto& adb_state = *state.adb_mode;
  {
    std::unique_lock state_lock(adb_state.mutex);
    std::unique_lock queue_lock(adb_state.queue_mutex);
    if (!adb_state.accepting_tasks || !adb_state.worker_thread.joinable()) {
      return false;
    }

    adb_state.operation_in_progress = true;
    adb_state.pending_tasks.fetch_add(1, std::memory_order_release);
    adb_state.task_queue.emplace(std::move(task));
  }
  adb_state.queue_cv.notify_one();
  return true;
}

// 将分辨率预设复制成后台计算需要的最小数据。
auto resolution_preset_at(const core::AppState& state, std::size_t index)
    -> features::window_control::ResolutionPresetInput {
  std::scoped_lock lock(state.settings->mutation_mutex);
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  if (index >= resolutions.size()) {
    return {};
  }
  return features::window_control::ResolutionPresetInput{
      .base_width = resolutions[index].base_width,
      .base_height = resolutions[index].base_height,
  };
}

// 在事件进入队列前复制当前比例，避免后台线程读取浮窗 UI 状态。
auto current_ratio_snapshot(const core::AppState& state) -> std::optional<double> {
  const auto current_index = state.floating_window->ui.current_ratio_index;
  std::scoped_lock lock(state.settings->mutation_mutex);
  const auto& ratios = features::settings::menu::get_ratios(state);
  if (current_index >= ratios.size()) {
    return std::nullopt;
  }
  return ratios[current_index].ratio;
}

// 复制窗口尺寸计算所需的两个设置开关。
// ADB 独立持有 use_resolution_long_edge，不再与窗口控制共用 use_resolution_short_edge。
auto get_adb_calculation_settings(const core::AppState& state) -> std::pair<bool, bool> {
  std::scoped_lock lock(state.settings->mutation_mutex);
  return {state.settings->raw.window.align_window_size_to_8,
          state.settings->raw.features.adb_mode.use_resolution_long_edge};
}

// 执行设备连接：状态切 Connecting → 构造并解析配置 → 打开完整设备会话 → 保存会话并广播状态
auto connect_impl(core::AppState& state) -> std::expected<void, std::string> {
  // 已经处于连接状态时直接返回
  if (is_connected(state)) {
    return {};
  }

  // 标记当前操作状态为连接中
  set_operation_state(state, ConnectionState::Connecting);

  // 解析并生成 ADB 连接配置（支持自动探测模拟器与自定义路径）
  auto config_result = build_connection_config(state);
  if (!config_result) {
    set_error_state(state, config_result.error(), false);
    publish_status_changed(state);
    return std::unexpected(config_result.error());
  }
  const auto config = config_result.value();

  // 建立完整的底层设备会话（连接、推服务、起进程、建立转发）
  auto session_result = session::open(config);
  if (!session_result) {
    set_error_state(state, session_result.error(), false);
    publish_status_changed(state);
    return std::unexpected(session_result.error());
  }

  // 在状态锁保护下更新连接成功状态与会话实例
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    state.adb_mode->connection_state = ConnectionState::Connected;
    state.adb_mode->device_session = std::move(session_result.value());
    state.adb_mode->restore_pending = false;
    state.adb_mode->last_error.clear();
  }

  // 广播连接成功事件并推送最新状态至前端
  post_connection_changed(state, true);
  publish_status_changed(state);
  return {};
}

// 恢复物理尺寸：状态切 Restoring → 区分断开或保留模式 → 恢复分辨率或关闭会话 → 广播连接变更
auto restore_impl(core::AppState& state, bool disconnect_after)
    -> std::expected<void, std::string> {
  // 检查当前是否有活动会话
  auto session_result = get_active_session(state);
  if (!session_result) {
    {
      std::scoped_lock lock(state.adb_mode->mutex);
      state.adb_mode->connection_state = ConnectionState::Disconnected;
      state.adb_mode->last_error.clear();
      clear_session_locked(*state.adb_mode);
    }
    if (disconnect_after) {
      post_connection_changed(state, false);
    }
    publish_status_changed(state);
    return {};
  }

  auto adb_session = session_result.value();
  // 标记当前状态为恢复中
  set_operation_state(state, ConnectionState::Restoring);

  // 若要求断开连接：关闭会话、移除端口转发并视情况断开 ADB
  if (disconnect_after) {
    if (is_recording(state)) {
      static_cast<void>(stop_recording(state));
    }
    auto close_result = session::close(adb_session);
    {
      std::scoped_lock lock(state.adb_mode->mutex);
      state.adb_mode->connection_state = ConnectionState::Disconnected;
      clear_session_locked(*state.adb_mode);
    }
    post_connection_changed(state, false);
    publish_status_changed(state);
    if (!close_result) {
      return std::unexpected(close_result.error());
    }
    return {};
  }

  // 仅恢复显示尺寸：调用 wm size reset
  auto reset_result = session::restore_display(*adb_session);
  if (!reset_result) {
    set_error_state(state, reset_result.error(), true);
    publish_status_changed(state);
    return std::unexpected(reset_result.error());
  }

  // 恢复成功后清除恢复标记
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    state.adb_mode->restore_pending = false;
    state.adb_mode->last_error.clear();
    state.adb_mode->connection_state = ConnectionState::Connected;
  }
  publish_status_changed(state);
  return {};
}

// 应用目标分辨率：参数校验 → 标记 restore_pending → 调 session 设置尺寸 → 延迟等待显示稳定
auto apply_resolution_impl(core::AppState& state, const Resolution& target)
    -> std::expected<void, std::string> {
  // 校验目标尺寸合法性
  if (target.width <= 0 || target.height <= 0) {
    return std::unexpected("ADB device display size must be positive");
  }

  // 获取当前活动会话
  auto session_result = get_active_session(state);
  if (!session_result) {
    return std::unexpected(session_result.error());
  }
  const auto adb_session = session_result.value();
  set_operation_state(state, ConnectionState::Connected);

  // 标记会话有待还原的尺寸更改
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    state.adb_mode->restore_pending = true;
  }

  // 通过底层会话执行 wm size override
  auto set_result = session::set_display(*adb_session, target);
  if (!set_result) {
    return std::unexpected("Failed to apply ADB device display settings: " + set_result.error());
  }

  // 短暂等待模拟器/系统完成布局刷新与帧缓冲重建
  std::this_thread::sleep_for(kDisplaySettleDelay);
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    state.adb_mode->last_error.clear();
  }
  return {};
}

}  // namespace

// 启动 ADB 专用串行线程，所有后续操作都通过它执行。
auto initialize(core::AppState& state) -> std::expected<void, std::string> {
  auto& adb_state = *state.adb_mode;
  {
    std::scoped_lock lock(adb_state.queue_mutex);
    if (adb_state.worker_thread.joinable()) {
      return {};
    }
    adb_state.accepting_tasks = true;
  }

  try {
    adb_state.worker_thread =
        std::jthread([&state](std::stop_token stop_token) { adb_thread_proc(state, stop_token); });
  } catch (const std::exception& e) {
    std::scoped_lock lock(adb_state.queue_mutex);
    adb_state.accepting_tasks = false;
    return std::unexpected(std::string("Failed to start ADB worker thread: ") + e.what());
  }
  return {};
}

// 按启动设置把自动连接任务放入 ADB 专用队列。
auto schedule_startup_tasks(core::AppState& state) -> void {
  if (!get_adb_mode_settings(state).auto_connect) {
    return;
  }

  if (!enqueue_task(state, [&state]() {
        auto result = connect_impl(state);
        post_operation_notification(state, result, "message.adb_connect_success",
                                    "message.adb_connect_failed");
      })) {
    Logger().warn("Startup ADB mode task was not scheduled");
  }
}

// 组装设置和运行时数据，返回前端需要的 ADB 模式状态快照。
auto get_status(const core::AppState& state) -> AdbModeStatus {
  const auto settings = get_adb_mode_settings(state);
  AdbModeStatus status{
      .connection_state = connection_state_to_string(ConnectionState::Disconnected),
      .adb_path = settings.adb_path,
      .host = settings.host,
      .port = settings.port,
  };

  const auto& adb_state = *state.adb_mode;
  std::scoped_lock lock(adb_state.mutex);
  status.operation_in_progress = adb_state.operation_in_progress;
  status.connection_state = connection_state_to_string(adb_state.connection_state);
  status.restore_pending = adb_state.restore_pending;
  status.last_error = adb_state.last_error;
  if (adb_state.device_session) {
    const auto& session = *adb_state.device_session;
    std::scoped_lock session_lock(session.metadata_mutex);
    status.connected = adb_state.connection_state == ConnectionState::Connected;
    status.adb_path = utils::string::ToUtf8(session.config.executable.wstring());
    status.serial = session.config.serial;
    status.host = session.config.host;
    status.port = session.config.port;
    status.display_width = session.current_display.width;
    status.display_height = session.current_display.height;
  }
  return status;
}

// 判断当前是否已建立可执行显示操作的连接。
auto is_connected(const core::AppState& state) -> bool {
  std::scoped_lock lock(state.adb_mode->mutex);
  return state.adb_mode->connection_state == ConnectionState::Connected &&
         state.adb_mode->device_session;
}

// 获取当前活动的设备会话句柄。
auto get_active_session(const core::AppState& state)
    -> std::expected<std::shared_ptr<session::DeviceSession>, std::string> {
  const auto& adb_state = *state.adb_mode;
  std::scoped_lock lock(adb_state.mutex);
  if (adb_state.connection_state != ConnectionState::Connected || !adb_state.device_session) {
    return std::unexpected("ADB device is not connected");
  }
  return adb_state.device_session;
}

// 启动 ADB 设备端屏幕与音频录制，通过 MF SinkWriter 混流写入 output_path。
auto start_recording(core::AppState& state, const std::filesystem::path& output_path,
                     std::uint32_t fps, std::uint32_t bitrate, bool is_h265)
    -> std::expected<void, std::string> {
  auto session_result = get_active_session(state);
  if (!session_result) {
    return std::unexpected(session_result.error());
  }

  auto& adb_state = *state.adb_mode;
  {
    std::scoped_lock lock(adb_state.mutex);
    if (adb_state.recording_session) {
      return std::unexpected("ADB recording session is already active");
    }
  }

  auto start_result = recording::start(*session_result, output_path, fps, bitrate, is_h265);
  if (!start_result) {
    return std::unexpected(start_result.error());
  }

  std::scoped_lock lock(adb_state.mutex);
  adb_state.recording_session = std::move(start_result.value());
  return {};
}

// 停止 ADB 设备录制，排空数据并完成 MP4 文件落盘。
auto stop_recording(core::AppState& state) -> recording::AdbRecordResult {
  std::unique_ptr<recording::AdbRecordingSession> session;
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    session = std::move(state.adb_mode->recording_session);
  }

  if (!session) {
    return recording::AdbRecordResult{.kind = recording::AdbRecordResult::Kind::NotRecording};
  }

  auto result = recording::stop(session);
  if (!result.error.empty()) {
    result.error = core::i18n::get_text(state.i18n->texts, result.error);
  }
  return result;
}

// 检查当前是否有活动的 ADB 录制会话。
auto is_recording(const core::AppState& state) -> bool {
  std::scoped_lock lock(state.adb_mode->mutex);
  return state.adb_mode->recording_session != nullptr;
}

// 异步设备截图：校验连接状态 → 投递截图任务至 ADB 队列 → 调用底层截图 → 更新错误状态 → 执行回调
auto capture_screen_async(
    core::AppState& state, const std::filesystem::path& output_path, AdbScreenshotFormat format,
    std::move_only_function<void(bool success, const std::wstring& path, std::string error)>
        completion_callback) -> bool {
  // 未连接时直接拒绝截图请求
  if (!is_connected(state)) {
    return false;
  }

  // 投递截图任务到后台工作队列中串行执行
  return enqueue_task(state, [&state, output_path, format,
                              completion_callback = std::move(completion_callback)]() mutable {
    // 获取活动会话
    auto session_result = get_active_session(state);
    std::expected<void, std::string> result = {};
    if (session_result) {
      // 调用长期会话的截图接口截取并保存为指定格式
      result = session::screenshot_to_file(*session_result.value(), output_path, format);
    } else {
      result = std::unexpected(session_result.error());
    }

    // 更新状态错误描述
    {
      std::scoped_lock lock(state.adb_mode->mutex);
      if (result) {
        state.adb_mode->last_error.clear();
      } else {
        state.adb_mode->last_error = result.error();
      }
    }
    publish_status_changed(state);

    // 执行完成回调通知上层业务
    if (completion_callback) {
      try {
        completion_callback(result.has_value(), output_path.wstring(),
                            result ? std::string{} : result.error());
      } catch (...) {
        Logger().error("ADB screenshot completion callback failed");
      }
    }
  });
}

// 把恢复物理尺寸任务放入队列，并在完成后同步浮窗菜单。
auto restore_async(core::AppState& state) -> bool {
  return enqueue_task(state, [&state]() {
    auto result = restore_impl(state, false);
    post_display_transform_result(state, std::nullopt, std::size_t{0}, result);
  });
}

// 根据当前连接状态把 ADB 模式切换任务放入队列。
auto toggle_async(core::AppState& state) -> bool {
  {
    std::scoped_lock lock(state.adb_mode->mutex);
    if (state.adb_mode->operation_in_progress) {
      return false;
    }
  }

  return enqueue_task(state, [&state]() {
    if (is_connected(state)) {
      auto result = restore_impl(state, true);
      post_operation_notification(state, result, "message.adb_disconnect_success",
                                  "message.adb_disconnect_failed");
      return;
    }

    auto result = connect_impl(state);
    post_operation_notification(state, result, "message.adb_connect_success",
                                "message.adb_connect_failed");
  });
}

// 设置变化后保持 ADB toggle 开启，按新配置恢复旧设备并连接新设备。
auto handle_settings_changed(core::AppState& state) -> void {
  if (!is_connected(state)) {
    return;
  }

  if (!enqueue_task(state, [&state]() {
        auto disconnect_result = restore_impl(state, true);
        if (!disconnect_result) {
          post_operation_notification(state, disconnect_result, "message.adb_disconnect_success",
                                      "message.adb_disconnect_failed");
          return;
        }

        auto connect_result = connect_impl(state);
        post_operation_notification(state, connect_result, "message.adb_connect_success",
                                    "message.adb_connect_failed");
      })) {
    Logger().warn("Failed to queue ADB reconnect after configuration change");
  }
}

// 关闭任务接收，等待队列完成后恢复物理尺寸并结束专用线程。
auto shutdown(core::AppState& state) -> void {
  auto& adb_state = *state.adb_mode;
  if (!adb_state.worker_thread.joinable()) {
    auto result = restore_impl(state, true);
    if (!result) {
      Logger().error("Failed to shutdown ADB mode: {}", result.error());
    }
    return;
  }

  {
    std::unique_lock state_lock(adb_state.mutex);
    std::unique_lock queue_lock(adb_state.queue_mutex);
    adb_state.accepting_tasks = false;
    adb_state.operation_in_progress = true;
    adb_state.pending_tasks.fetch_add(1, std::memory_order_release);
    adb_state.task_queue.emplace([&state]() {
      auto result = restore_impl(state, true);
      if (!result) {
        Logger().error("Failed to restore ADB display during shutdown: {}", result.error());
      }
    });
  }

  adb_state.worker_thread.request_stop();
  adb_state.queue_cv.notify_one();
  if (adb_state.worker_thread.joinable()) {
    adb_state.worker_thread.join();
  }
}

// 依据新的比例和当前分辨率预设计算目标尺寸，再异步应用到设备。
auto handle_ratio_changed(core::AppState& state, std::size_t ratio_index, double ratio_value)
    -> void {
  const auto current_resolution_index = state.floating_window->ui.current_resolution_index;
  const auto resolution_preset = resolution_preset_at(state, current_resolution_index);
  const auto [align_to_8, use_long_edge] = get_adb_calculation_settings(state);

  if (!enqueue_task(state, [&state, ratio_index, ratio_value, current_resolution_index,
                            resolution_preset, align_to_8, use_long_edge]() {
        auto session_result = get_active_session(state);
        if (!session_result) {
          const std::expected<void, std::string> failure = std::unexpected(session_result.error());
          post_display_transform_result(state, ratio_index, std::nullopt, failure);
          return;
        }

        auto preset = resolution_preset;
        if (current_resolution_index == 0 || (preset.base_width <= 0 && preset.base_height <= 0)) {
          preset = features::window_control::ResolutionPresetInput{
              .base_width = session_result.value()->physical_display.width,
              .base_height = session_result.value()->physical_display.height,
          };
        }

        const auto target = display_control::calculate_target_resolution(
            ratio_value, preset, session_result.value()->physical_display, align_to_8,
            use_long_edge);
        auto result = apply_resolution_impl(state, target);
        if (!result) {
          set_error_state(state, result.error(), true);
        }
        publish_status_changed(state);
        post_display_transform_result(state, ratio_index, std::nullopt, result);
      })) {
    post_queue_failure_notification(state);
  }
}

// 依据新的分辨率预设和当前比例计算目标尺寸，再异步应用到设备。
auto handle_resolution_changed(core::AppState& state, std::size_t resolution_index) -> void {
  if (resolution_index == 0) {
    if (!restore_async(state)) {
      post_queue_failure_notification(state);
    }
    return;
  }

  const auto resolution_preset = resolution_preset_at(state, resolution_index);
  const auto ratio = current_ratio_snapshot(state);
  const auto [align_to_8, use_long_edge] = get_adb_calculation_settings(state);

  if (!enqueue_task(state, [&state, resolution_index, resolution_preset, ratio, align_to_8,
                            use_long_edge]() {
        auto session_result = get_active_session(state);
        if (!session_result) {
          const std::expected<void, std::string> failure = std::unexpected(session_result.error());
          post_display_transform_result(state, std::nullopt, resolution_index, failure);
          return;
        }

        const auto base = session_result.value()->physical_display;
        const auto current_ratio = ratio.value_or(
            base.width > 0 && base.height > 0 ? static_cast<double>(base.width) / base.height
                                              : 16.0 / 9.0);
        const auto target = display_control::calculate_target_resolution(
            current_ratio, resolution_preset, base, align_to_8, use_long_edge);
        auto result = apply_resolution_impl(state, target);
        if (!result) {
          set_error_state(state, result.error(), true);
        }
        publish_status_changed(state);
        post_display_transform_result(state, std::nullopt, resolution_index, result);
      })) {
    post_queue_failure_notification(state);
  }
}

// 发现所有当前可用的 Android 设备（包括运行中模拟器与已识别的真机）。
auto list_devices(core::AppState& state)
    -> std::expected<std::vector<DiscoveredAdbDevice>, std::string> {
  const auto settings = get_adb_mode_settings(state);
  std::filesystem::path executable;
  if (settings.use_custom_adb_path && !settings.adb_path.empty()) {
    executable = std::filesystem::path(utils::string::FromUtf8(settings.adb_path));
  }

  return device_finder::discover_all_devices(AdbConnectionConfig{
      .executable = std::move(executable),
      .host = settings.host,
      .port = settings.port,
      .serial = settings.serial,
  });
}

// 手动连接网络端点（用于无线调试或未自动探测到的模拟器）。
auto connect_endpoint(core::AppState& state, std::string host, int port)
    -> std::expected<ConnectEndpointResult, std::string> {
  if (host.empty()) {
    return std::unexpected("Host cannot be empty");
  }
  if (port <= 0 || port > 65535) {
    return std::unexpected("Port must be between 1 and 65535");
  }

  const auto settings = get_adb_mode_settings(state);
  std::filesystem::path executable;
  if (settings.use_custom_adb_path && !settings.adb_path.empty()) {
    executable = std::filesystem::path(utils::string::FromUtf8(settings.adb_path));
  }

  AdbConnectionConfig config{
      .executable = std::move(executable),
      .host = host,
      .port = port,
  };

  // 补齐可执行文件
  auto config_result = device_finder::resolve_connection(config);
  if (!config_result) {
    return ConnectEndpointResult{
        .success = false,
        .serial = "",
        .error = config_result.error(),
    };
  }

  const auto endpoint = std::format("{}:{}", host, port);
  auto connect_result =
      adb::connect_endpoint(config_result.value(), endpoint, std::chrono::seconds(5));
  if (!connect_result) {
    return ConnectEndpointResult{
        .success = false,
        .serial = endpoint,
        .error = connect_result.error(),
    };
  }

  return ConnectEndpointResult{
      .success = true,
      .serial = endpoint,
      .error = "",
  };
}

}  // namespace features::adb_mode
