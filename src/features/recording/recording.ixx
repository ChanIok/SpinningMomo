module;

export module Features.Recording;

import std;
import Core.State;
import Features.Recording.Types;
import Features.Recording.State;
import <windows.h>;

export namespace Features::Recording {

struct RecordingControlHandlers {
  std::function<void()> on_toggle;
  std::function<void()> on_shutdown_stop;
};

// 初始化录制模块
auto initialize(Features::Recording::State::RecordingState& state)
    -> std::expected<void, std::string>;

// 首次需要录制控制面时启动常驻控制线程
auto ensure_control_thread_started(Core::State::AppState& app_state,
                                   Features::Recording::State::RecordingState& state,
                                   RecordingControlHandlers handlers)
    -> std::expected<void, std::string>;

// 提交录制控制请求；真正的 start/stop/restart 只在控制线程执行
auto request_control_action(Features::Recording::State::RecordingState& state,
                            Features::Recording::State::RecordingControlAction action) -> bool;

// 等待录制控制线程退出
auto join_control_thread(Features::Recording::State::RecordingState& state) -> void;

// 开始录制
auto start(Core::State::AppState& app_state, Features::Recording::State::RecordingState& state,
           HWND target_window, const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;

// 停止录制
auto stop(Features::Recording::State::RecordingState& state) -> void;

// 请求一次尺寸变化后的自动切段重启（只提交请求，不直接执行重操作）
auto request_restart_after_resize(Features::Recording::State::RecordingState& state) -> void;

// 执行尺寸变化后的自动切段重启；只能由录制控制线程调用
auto restart_after_resize(Core::State::AppState& app_state,
                          Features::Recording::State::RecordingState& state) -> void;

// 清理资源
auto cleanup(Features::Recording::State::RecordingState& state) -> void;

}  // namespace Features::Recording
