#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace Features::Preview {

// 开始捕获并显示预览
auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string>;

// 停止预览并隐藏窗口
auto stop_preview(Core::State::AppState& state) -> void;

// DPI 处理
auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void;

// 清理资源
auto cleanup_preview(Core::State::AppState& state) -> void;

}  // namespace Features::Preview