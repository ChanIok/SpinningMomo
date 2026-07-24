#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::recording {

// 响应用户录制开关请求，负责查找窗口、组装配置并提交控制动作。
auto toggle_recording(core::AppState& state) -> std::expected<void, std::string>;

// 应用关闭时停止正在运行的录制，并等待控制线程完成收尾。
auto stop_recording_if_running(core::AppState& state) -> void;

}  // namespace features::recording
