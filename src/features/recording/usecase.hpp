#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::recording {

// 响应用户录制开关请求，负责查找窗口、组装配置并提交控制动作。
auto toggle_recording(core::AppState& state) -> std::expected<void, std::string>;

// 桌面与 ADB 共用保存通知及查看动作。
auto notify_recording_saved(core::AppState& state, const std::filesystem::path& saved_path) -> void;

// 应用关闭时关闭 ADB 后端并停止桌面录制，等待各自的线程完成收尾。
auto stop_recording_if_running(core::AppState& state) -> void;

// 完成停止流程后释放录制设备与 Media Foundation 运行时。
auto cleanup(core::AppState& state) -> void;

}  // namespace features::recording
