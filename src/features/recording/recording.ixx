module;

export module Features.Recording;

import std;
import Core.State;
import Features.Recording.Types;
import Features.Recording.State;
import <windows.h>;

namespace Features::Recording {

export struct RecordingControlHandlers {
  std::function<void()> on_toggle;
  std::function<void()> on_shutdown_stop;
};

// 初始化录制模块依赖的 Media Foundation 运行时。
export auto initialize(Features::Recording::State::RecordingState& state)
    -> std::expected<void, std::string>;

// 首次需要录制控制面时启动常驻控制线程，后续复用同一个线程处理请求。
export auto ensure_control_thread_started(Core::State::AppState& app_state,
                                          Features::Recording::State::RecordingState& state,
                                          RecordingControlHandlers handlers)
    -> std::expected<void, std::string>;

// 提交录制控制请求；真正的 start/stop/restart 只在控制线程执行。
export auto request_control_action(Features::Recording::State::RecordingState& state,
                                   Features::Recording::State::RecordingControlAction action)
    -> bool;

// 等待录制控制线程退出，用于应用关闭阶段完成录制收尾。
export auto join_control_thread(Features::Recording::State::RecordingState& state) -> void;

// 开始一个新的录制段，负责准备资源、启动编码线程、启动 WGC 和音频采集。
export auto start(Core::State::AppState& app_state,
                  Features::Recording::State::RecordingState& state, HWND target_window,
                  const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;

// 停止当前录制段，排空输入、finalize 编码器并发布或删除临时文件。
export auto stop(Features::Recording::State::RecordingState& state) -> void;

// 请求一次尺寸变化后的自动切段重启；只提交请求，不直接执行重操作。
export auto request_restart_after_resize(Features::Recording::State::RecordingState& state) -> void;

// 执行尺寸变化后的自动切段重启；只能由录制控制线程调用。
export auto restart_after_resize(Core::State::AppState& app_state,
                                 Features::Recording::State::RecordingState& state) -> void;

// 清理录制模块资源，并关闭 Media Foundation 运行时。
export auto cleanup(Features::Recording::State::RecordingState& state) -> void;

}  // namespace Features::Recording
