#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace features::preview::capture {

// 初始化捕获系统
auto initialize_capture(core::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string>;

// 开始捕获
auto start_capture(core::AppState& state) -> std::expected<void, std::string>;

// 记录捕获尺寸变化，并在尺寸稳定后重建帧池。
auto queue_resize(core::AppState& state, int width, int height) -> std::expected<void, std::string>;

// 应用延迟期间记录的尺寸变化，重建当前帧池。
auto apply_pending_resize(core::AppState& state) -> std::expected<void, std::string>;

// 停止捕获
auto stop_capture(core::AppState& state) -> void;

// 清理捕获资源
auto cleanup_capture(core::AppState& state) -> void;

}  // namespace features::preview::capture
