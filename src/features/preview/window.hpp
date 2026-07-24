#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"
#include "features/preview/state.hpp"

namespace Features::Preview::Window {

// 显示窗口
auto show_preview_window(Core::State::AppState& state) -> void;

// 隐藏窗口
auto hide_preview_window(Core::State::AppState& state) -> void;

// 更新DPI
auto update_preview_window_dpi(Core::State::AppState& state, UINT new_dpi) -> void;

// 销毁窗口
auto destroy_preview_window(Core::State::AppState& state) -> void;

// 计算窗口尺寸
auto set_preview_window_size(Core::State::AppState& app_state, int capture_width,
                             int capture_height) -> void;

// 初始化预览窗口系统
auto initialize_preview_window(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

}  // namespace Features::Preview::Window