#pragma once

#include "core/state/app_state.hpp"
#include "vendor/windows.hpp"

namespace Features::Overlay::Capture {

// 初始化捕获系统
auto initialize_capture(Core::State::AppState& state, Vendor::Windows::HWND target_window,
                        int width, int height) -> std::expected<void, std::string>;

// 开始捕获
auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止捕获
auto stop_capture(Core::State::AppState& state) -> void;

// 清理捕获资源
auto cleanup_capture(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::Capture
