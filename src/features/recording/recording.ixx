module;

export module Features.Recording;

import std;
import Core.State;
import Features.Recording.Types;
import <windows.h>;

namespace Features::Recording {

// 初始化录制模块依赖的 Media Foundation 运行时。
export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

// 首次需要录制控制面时启动常驻控制线程，后续复用同一个线程处理请求。
export auto ensure_control_thread_started(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 提交录制控制请求；真正的 start/stop/restart 只在控制线程执行。
export auto request_control_action(Core::State::AppState& app_state,
                                   Features::Recording::Types::RecordingControlAction action)
    -> bool;

// 等待录制控制线程退出，用于应用关闭阶段完成录制收尾。
export auto join_control_thread(Core::State::AppState& app_state) -> void;

// 开始一个新的录制段，负责准备资源、启动编码线程、启动 WGC 和音频采集。
export auto start(Core::State::AppState& app_state, HWND target_window,
                  const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;

// 停止当前录制段，排空输入、finalize 编码器并发布或删除临时文件。
export auto stop(Core::State::AppState& app_state) -> Features::Recording::Types::StopResult;

// 请求一次尺寸变化后的自动切段重启；只提交请求，不直接执行重操作。
export auto request_restart_after_resize(Core::State::AppState& app_state) -> void;

// 执行尺寸变化后的自动切段重启；只能由录制控制线程调用。
export auto restart_after_resize(Core::State::AppState& app_state) -> void;

// 清理录制模块资源，并关闭 Media Foundation 运行时。
export auto cleanup(Core::State::AppState& app_state) -> void;

}  // namespace Features::Recording
