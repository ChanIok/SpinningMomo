module;

#include <windows.h>

export module Features.Overlay.Window;

import std;
import Features.Overlay.State;
import Core.State;

namespace Features::Overlay::Window {

// 创建叠加层窗口
export auto create_overlay_window(HINSTANCE instance, Core::State::AppState& state)
    -> std::expected<HWND, std::string>;

// 初始化叠加层窗口系统
export auto initialize_overlay_window(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

// 显示叠加层窗口
export auto show_overlay_window(Core::State::AppState& state) -> void;

// 隐藏叠加层窗口
export auto hide_overlay_window(Core::State::AppState& state) -> void;

// 更新叠加层窗口尺寸
export auto set_overlay_window_size(Core::State::AppState& state, int game_width, int game_height)
    -> std::expected<void, std::string>;

// 销毁叠加层窗口
export auto destroy_overlay_window(Core::State::AppState& state) -> void;

// 注销叠加层窗口类
export auto unregister_overlay_window_class(HINSTANCE instance) -> void;

// 恢复游戏窗口
export auto restore_game_window(Core::State::AppState& state, bool with_delay = false) -> void;

}  // namespace Features::Overlay::Window
