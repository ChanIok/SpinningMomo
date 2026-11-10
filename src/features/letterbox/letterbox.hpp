#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "utils/display/display.hpp"

namespace features::letterbox {

// 初始化和清理
auto initialize(core::AppState& state, HINSTANCE instance) -> std::expected<void, std::string>;

auto shutdown(core::AppState& state) -> std::expected<void, std::string>;

// 显式启动或调整黑边，目标窗口和屏幕均由调用方确定。
auto show(core::AppState& state, HWND target_window,
          const utils::display::MonitorInfo& working_monitor) -> std::expected<void, std::string>;

auto hide(core::AppState& state) -> std::expected<void, std::string>;

}  // namespace features::letterbox
