#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "features/recording/types.hpp"

namespace features::recording {

// 初始化录制模块依赖的 Media Foundation 运行时。
auto initialize(core::AppState& app_state) -> std::expected<void, std::string>;

// 提交录制控制请求；真正的 start/stop/restart 只在控制线程执行。
auto request_control_action(core::AppState& app_state,
                            features::recording::RecordingControlAction action) -> bool;

// 抢占当前录制段的停止权；成功后状态立即切到 Stopping。
auto enter_stopping(core::AppState& app_state) -> bool;

// 开始一个新的录制段，负责准备资源、启动编码线程、启动 WGC 和音频采集。
auto start(core::AppState& app_state, HWND target_window,
           const features::recording::RecordingConfig& config) -> std::expected<void, std::string>;

// 停止 Recording 或已进入 Stopping 的当前段，排空编码、补全 MP4 索引。
// 异常录像保留，仅误触短段删除；图库与通知由 usecase 处理。
auto stop(core::AppState& app_state) -> features::recording::StopResult;

}  // namespace features::recording
