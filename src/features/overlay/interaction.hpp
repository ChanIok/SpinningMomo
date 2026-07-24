#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace Features::Overlay::Interaction {

// 初始化交互系统（钩子等）
auto initialize_interaction(Core::State::AppState& state) -> std::expected<void, std::string>;

// 处理叠加层窗口消息
auto handle_overlay_message(Core::State::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<bool, LRESULT>;

// 安装窗口事件钩子
auto install_window_event_hook(Core::State::AppState& state) -> std::expected<void, std::string>;

// 卸载所有钩子
auto uninstall_hooks(Core::State::AppState& state) -> void;

// 更新游戏窗口位置
auto update_game_window_position(Core::State::AppState& state) -> void;

// 处理窗口事件
auto handle_window_event(Core::State::AppState& state, DWORD event, HWND hwnd) -> void;

// 同步当前前台窗口对应的焦点状态
auto refresh_focus_state(Core::State::AppState& state) -> void;

// 清理交互资源
auto cleanup_interaction(Core::State::AppState& state) -> void;

// 禁止任务栏重绘
auto suppress_taskbar_redraw(Core::State::AppState& state) -> void;

// 恢复任务栏重绘
auto restore_taskbar_redraw(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::Interaction
