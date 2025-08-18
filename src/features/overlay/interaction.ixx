module;

#include <windows.h>

export module Features.Overlay.Interaction;

import std;
import Core.State;

namespace Features::Overlay::Interaction {

// 初始化交互系统（钩子等）
export auto initialize_interaction(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 处理叠加层窗口消息
export auto handle_overlay_message(Core::State::AppState& state, HWND hwnd, UINT message,
                                   WPARAM wParam, LPARAM lParam) -> std::pair<bool, LRESULT>;

// 安装鼠标钩子
export auto install_mouse_hook(Core::State::AppState& state) -> std::expected<void, std::string>;

// 安装窗口事件钩子
export auto install_window_event_hook(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 卸载所有钩子
export auto uninstall_hooks(Core::State::AppState& state) -> void;

// 处理鼠标移动
export auto handle_mouse_movement(Core::State::AppState& state, POINT mouse_pos) -> void;

// 更新游戏窗口位置
export auto update_game_window_position(Core::State::AppState& state) -> void;

// 处理窗口事件
export auto handle_window_event(Core::State::AppState& state, DWORD event, HWND hwnd) -> void;

// 清理交互资源
export auto cleanup_interaction(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::Interaction
