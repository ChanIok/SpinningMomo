#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace features::letterbox {

// 初始化和清理
auto initialize(core::AppState& state, HINSTANCE instance) -> std::expected<void, std::string>;

auto shutdown(core::AppState& state) -> std::expected<void, std::string>;

// 窗口操作
auto show(core::AppState& state, HWND target_window = nullptr) -> std::expected<void, std::string>;

auto hide(core::AppState& state) -> std::expected<void, std::string>;

}  // namespace features::letterbox
