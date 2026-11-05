#include "features/recording/recording.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/timeapi.hpp"
#include "vendor/windows/winrt/windows_graphics_capture.hpp"

#include "features/recording/audio.hpp"
#include "features/recording/mux.hpp"
#include "features/recording/state.hpp"
#include "features/recording/time.hpp"
#include "features/recording/video.hpp"
#include "features/settings/state.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::recording {
namespace {

// 计算控制动作的抢占优先级：ShutdownStop > AbortWithError > UserStop > RestartAfterResize >
// UserStart
auto action_priority(RecordingControlAction action) -> int {
  switch (action) {
    case RecordingControlAction::ShutdownStop:
      return 5;
    case RecordingControlAction::AbortWithError:
      return 4;
    case RecordingControlAction::UserStop:
      return 3;
    case RecordingControlAction::RestartAfterResize:
      return 2;
    case RecordingControlAction::UserStart:
      return 1;
    default:
      return 0;
  }
}

// 等待所有工作线程退出并恢复系统时钟中断精度
auto join_workers(RecordingSession& session) -> void {
  // 等待视频处理线程退出
  if (session.video_thread.joinable()) {
    session.video_thread.join();
  }

  // 等待音频采集与编码线程退出
  if (session.audio_thread.joinable()) {
    session.audio_thread.join();
  }

  // 等待封装输出线程退出
  if (session.mux_thread.joinable()) {
    session.mux_thread.join();
  }

  // 恢复之前请求的 1ms 时钟中断精度
  if (session.timer_period_requested) {
    timeEndPeriod(1);
    session.timer_period_requested = false;
  }
}

// 封装工作线程通用上下文：初始化 MTA COM 环境 → 执行任务 → 捕获异常标记失败 → 通知完成
auto run_worker(core::AppState& app_state, UINT stream,
                std::expected<void, std::string> (*run)(core::AppState&)) -> void {
  auto& session = *app_state.recording->session;

  // 初始化工作线程的 COM MTA 套间
  const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(com)) {
    fail_session(app_state, utils::media::mft::hr_error("Worker COM initialization", com));
  } else {
    auto cleanup = wil::scope_exit([] { CoUninitialize(); });

    // 执行具体的工作线程任务逻辑并捕获可能出现的异常
    try {
      auto result = run(app_state);
      if (!result) {
        fail_session(app_state, result.error());
      }
    } catch (const winrt::hresult_error& error) {
      fail_session(app_state, utils::string::ToUtf8(error.message().c_str()));
    } catch (const std::exception& error) {
      fail_session(app_state, error.what());
    }
  }

  // 标记该流（视频/音频）已完成工作，通知封装线程
  if (stream < 2) {
    std::lock_guard lock(session.mutex);
    session.streams[stream].done = true;
    session.changed.notify_all();
  }
}

}  // namespace

// 请求会话停止：原子写入 QPC 截止时间戳并唤醒等待中的线程
auto request_session_stop(core::AppState& app_state, std::int64_t qpc_100ns) -> void {
  auto& session = *app_state.recording->session;
  std::int64_t empty = -1;

  // 首次请求时记录截止时间戳，后续请求不覆盖
  session.stop_qpc_100ns.compare_exchange_strong(empty, qpc_100ns);
  session.changed.notify_all();
}

// 标记会话失败：记录首个错误、打印日志并广播停止
auto fail_session(core::AppState& app_state, std::string error) -> void {
  auto& session = *app_state.recording->session;
  {
    std::lock_guard lock(session.mutex);

    // 已经处于失败态时不再覆盖第一个触发失败的原始原因
    if (session.failed.load()) {
      return;
    }

    session.error = std::move(error);
    session.failed.store(true);
  }

  Logger().error("Recording failed: {}; stages={}/{}/{}", session.error, session.video_stage.load(),
                 session.audio_stage.load(), session.mux_stage.load());

  // 触发停止并唤醒各工作线程排空并退出
  request_session_stop(app_state, time::query_qpc_100ns());

  // 启动或录制期间的失败交给控制线程收尾；已在停止时不重复提交
  const auto status = app_state.recording->status.load();
  if (status == RecordingStatus::Recording || status == RecordingStatus::Starting) {
    request_control_action(app_state, RecordingControlAction::AbortWithError);
  }
}

// 等待会话启动完成：阻塞直到就绪、失败或被中止
auto wait_session_start(core::AppState& app_state) -> bool {
  auto& session = *app_state.recording->session;
  std::unique_lock lock(session.mutex);

  // 等待启动成功、发生失败或请求了停止
  session.changed.wait(lock, [&] {
    return session.started.load() || session.failed.load() || session.stop_qpc_100ns.load() >= 0;
  });

  return session.started.load() && !session.failed.load() && session.stop_qpc_100ns.load() < 0;
}

// 初始化录制模块依赖的 Media Foundation 运行时
auto initialize(core::AppState&) -> std::expected<void, std::string> {
  const auto hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    return std::unexpected(utils::media::mft::hr_error("MFStartup", hr));
  }
  return {};
}

// 提交录制控制请求：校验动作优先级 → 记录时间戳 → 唤醒控制线程
auto request_control_action(core::AppState& app_state, RecordingControlAction action) -> bool {
  auto& state = *app_state.recording;

  // shutdown 后只接受 ShutdownStop，其他请求一律拒绝
  if (state.shutdown_requested.load() && action != RecordingControlAction::ShutdownStop) {
    return false;
  }

  std::lock_guard lock(state.control_request_mutex);

  // 低优先级动作不能覆盖正在挂起的高优先级动作
  if (action_priority(action) < action_priority(state.pending_action)) {
    return false;
  }

  // 发生在 UI/来源回调提交请求的时刻，不把控制线程排队和关闭设备的耗时录进去
  if (action != RecordingControlAction::UserStart && action != RecordingControlAction::None &&
      state.session) {
    request_session_stop(app_state, time::query_qpc_100ns());
  }

  // 更新挂起的动作并唤醒后台控制线程
  state.pending_action = action;
  state.control_cv.notify_one();
  return true;
}

// 抢占停止状态：由 Recording 切入 Stopping，同时锁定停止时间戳
auto enter_stopping(core::AppState& app_state) -> bool {
  auto& state = *app_state.recording;
  std::lock_guard lock(state.control_request_mutex);

  // 只有正处于录制中的状态才能转入停止态
  auto expected = RecordingStatus::Recording;
  if (!state.status.compare_exchange_strong(expected, RecordingStatus::Stopping)) {
    return false;
  }

  // 立即锁定会话截止点
  if (state.session) {
    request_session_stop(app_state, time::query_qpc_100ns());
  }
  return true;
}

// 启动一个录制段：算尺寸 → 配置会话 → 起定时精度 → 起音视频及封装线程 → 等待流水线就绪
auto start(core::AppState& app_state, HWND window, const RecordingConfig& config)
    -> std::expected<void, std::string> {
  auto& state = *app_state.recording;

  // 拒绝在已有活动会话时重复启动
  if (state.session) {
    return std::unexpected("Recording session already exists");
  }

  state.status.store(RecordingStatus::Starting);

  // 校验目标帧率
  if (!config.fps) {
    return std::unexpected("Recording FPS must be positive");
  }

  // 检查目标窗口是否有效且未最小化
  if (IsIconic(window) || !IsWindow(window)) {
    return std::unexpected("Recording target is unavailable");
  }

  // HDR 模式目前要求硬件 H.265 编码器
  if (config.enable_hdr &&
      (config.codec != VideoCodec::H265 || config.encoder_mode == EncoderMode::CPU)) {
    return std::unexpected("HDR recording requires hardware H.265");
  }

  // 检查输出文件路径是否已存在
  std::error_code error;
  if (std::filesystem::exists(config.output_path, error)) {
    return std::unexpected("Recording output already exists");
  }
  if (error) {
    return std::unexpected(error.message());
  }

  // 计算捕获范围与裁剪尺寸
  auto size = utils::graphics::capture::get_capture_item_size(window);
  if (!size) {
    return std::unexpected(size.error());
  }

  auto plan =
      video::resolve_capture_plan(window, config.capture_client_area, size->first, size->second);
  if (!plan) {
    return std::unexpected(plan.error());
  }

  // 原地构造本段会话；从发布到线程全部退出期间，会话地址保持不变
  {
    std::lock_guard lock(state.control_request_mutex);
    state.session.emplace();
  }
  auto& session = *state.session;

  // 启动失败时的作用域清理守卫：停止各线程、清理空输出文件并重置会话
  auto failed_start = wil::scope_exit([&] {
    if (session.started.load() && !session.failed.load()) {
      return;
    }

    request_session_stop(app_state, time::query_qpc_100ns());
    session.failed.store(true);
    session.changed.notify_all();
    join_workers(session);

    // 本次还没有产生有效画面时才清理自己的空文件；异常有效分片保留
    if (!session.video_encoded.load() && session.output_created.load()) {
      std::error_code ignored;
      std::filesystem::remove(config.output_path, ignored);
    }

    std::lock_guard lock(state.control_request_mutex);
    state.session.reset();
  });

  // 初始化会话配置，运行期间不再跟随全局设置变化
  session.config = config;
  session.config.width = plan->output_width;
  session.config.height = plan->output_height;
  session.target_window = window;

  {
    std::lock_guard lock(app_state.settings->mutation_mutex);
    session.prefer_high_performance_gpu = app_state.settings->raw.app.prefer_high_performance_gpu;
  }

  // 提升系统时钟中断精度至 1ms，保证帧节拍准时
  if (timeBeginPeriod(1) != TIMERR_NOERROR) {
    return std::unexpected("Cannot request recording timer precision");
  }
  session.timer_period_requested = true;

  // 准备期间可能已收到终止请求，启动工作线程前先冻结截止点
  {
    std::lock_guard lock(state.control_request_mutex);
    if (state.shutdown_requested.load() ||
        state.pending_action == RecordingControlAction::UserStop ||
        state.pending_action == RecordingControlAction::AbortWithError ||
        state.pending_action == RecordingControlAction::ShutdownStop) {
      request_session_stop(app_state, time::query_qpc_100ns());
    }
  }

  // 启动视频处理线程（stream 0）
  session.video_thread = std::jthread([&app_state] { run_worker(app_state, 0, video::run); });

  // 启动音频处理线程（stream 1），若未开启音频则直接标记就绪
  if (config.audio_source != utils::media::audio_capture::AudioSource::None) {
    session.audio_thread = std::jthread([&app_state] { run_worker(app_state, 1, audio::run); });
  } else {
    std::lock_guard lock(session.mutex);
    session.streams[1].ready = session.streams[1].done = true;
  }

  // 启动封装线程（stream 2）
  session.mux_thread = std::jthread([&app_state] { run_worker(app_state, 2, mux::run); });

  // 等待各流就绪（限时 15 秒）
  {
    std::unique_lock lock(session.mutex);
    if (!session.changed.wait_for(lock, std::chrono::seconds(15), [&] {
          return session.failed.load() || session.stop_qpc_100ns.load() >= 0 ||
                 (session.streams[0].ready && session.streams[1].ready);
        })) {
      lock.unlock();
      fail_session(app_state, "Recording startup exceeded 15 seconds");
      lock.lock();
    }

    if (session.failed.load()) {
      return std::unexpected(session.error);
    }
    if (session.stop_qpc_100ns.load() >= 0) {
      return std::unexpected("Recording startup cancelled");
    }

    // 记录录制正式开始的绝对时间戳
    session.start_qpc_100ns = time::query_qpc_100ns();
    if (!session.start_qpc_100ns) {
      return std::unexpected("Cannot read recording clock");
    }

    session.started.store(true);
    state.status.store(RecordingStatus::Recording);
  }

  session.changed.notify_all();
  failed_start.release();

  Logger().info("Recording started: {}", config.output_path.string());
  return {};
}

// 停止录制段：截断截止时间戳 → 等待工作线程退出 → 判定是否丢弃过短录像 → 释放会话
auto stop(core::AppState& app_state) -> StopResult {
  auto& state = *app_state.recording;
  if (!state.session) {
    return {};
  }

  state.status.store(RecordingStatus::Stopping);
  auto& session = *state.session;

  // 广播停止截止点
  request_session_stop(app_state, time::query_qpc_100ns());
  const auto began = time::query_qpc_100ns();

  Logger().info("Recording stop requested: stages={}/{}/{}", session.video_stage.load(),
                session.audio_stage.load(), session.mux_stage.load());

  // 等待全部工作线程完成排空并退出
  join_workers(session);

  // 组装停止结果并校验视频编码总帧数
  StopResult result;
  result.output_path = session.config.output_path;
  result.error = session.error;

  if (!result.error.empty()) {
    result.kind = StopResultKind::Failed;
  } else if (session.video_encoded.load() <= k_discard_video_frame_threshold) {
    // 画面少于等于阈值（3帧）判定为误触短录，丢弃并删除文件
    result.kind = StopResultKind::Discarded;
    result.error = "Recording segment contains no more than three video frames";

    std::error_code error;
    std::filesystem::remove(result.output_path, error);
    if (error) {
      result.kind = StopResultKind::Failed;
      result.error = error.message();
    }
  } else {
    result.kind = StopResultKind::Saved;
  }

  Logger().info(
      "Recording stopped: join={:.3f}ms video={}/{} skipped={} audio={} silence_frames={} "
      "discarded_audio_frames={} pool_peak={} packet_peak={}",
      (time::query_qpc_100ns() - began) / 10000.0, session.video_encoded.load(),
      session.video_submitted.load(), session.video_skipped.load(), session.audio_encoded.load(),
      session.audio_silent_frames.load(), session.audio_discarded_frames.load(),
      session.pool_peak.load(), session.peak_packet_bytes);

  // 销毁会话并恢复 Idle 状态
  {
    std::lock_guard lock(state.control_request_mutex);
    state.session.reset();
    state.status.store(RecordingStatus::Idle);
  }
  return result;
}

}  // namespace features::recording
