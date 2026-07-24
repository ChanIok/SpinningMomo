#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace features::preview {

// 开始捕获并显示预览
auto start_preview(core::AppState& state, HWND target_window) -> std::expected<void, std::string>;

// 停止预览并隐藏窗口
auto stop_preview(core::AppState& state) -> void;

// DPI 处理
auto update_preview_dpi(core::AppState& state, UINT new_dpi) -> void;

// 清理资源
auto cleanup_preview(core::AppState& state) -> void;

}  // namespace features::preview
