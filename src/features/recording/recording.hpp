#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "features/recording/types.hpp"

namespace features::recording {

// 以下会话操作要求活动会话在调用期间存活；控制线程在全部工作线程退出后才销毁会话。

// 标记会话失败并记录首个错误原因
auto fail_session(core::AppState& app_state, std::string error) -> void;

// 向会话请求停止并广播截止时间戳
auto request_session_stop(core::AppState& app_state, std::int64_t qpc_100ns) -> void;

// 阻塞等待会话完成初始化并正式进入录制态
auto wait_session_start(core::AppState& app_state) -> bool;

// 初始化录制模块依赖的 Media Foundation 运行时
auto initialize(core::AppState& app_state) -> std::expected<void, std::string>;

// 提交录制控制请求；真正的 start/stop/restart 只在控制线程执行
auto request_control_action(core::AppState& app_state,
                            features::recording::RecordingControlAction action) -> bool;

// 抢占当前录制段的停止权；成功后状态立即切到 Stopping
auto enter_stopping(core::AppState& app_state) -> bool;

// 创建独占资源的单段会话，启动视频、音频和压缩封装处理线
auto start(core::AppState& app_state, HWND target_window,
           const features::recording::RecordingConfig& config) -> std::expected<void, std::string>;

// 停止当前段：完成截止点前的有限工作和 MP4 索引，不补历史视频帧
auto stop(core::AppState& app_state) -> features::recording::StopResult;

}  // namespace features::recording
