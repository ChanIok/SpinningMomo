module;

#include <windows.h>

export module Features.Overlay.Window;

import std;
import Features.Overlay.State;
import Core.State;

namespace Features::Overlay::Window {

// 创建叠加层窗口
export auto create_overlay_window(HINSTANCE instance, HWND parent, Core::State::AppState& state)
    -> std::expected<HWND, std::string>;

// 初始化叠加层窗口系统
export auto initialize_overlay_window(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string>;

// 显示叠加层窗口
export auto show_overlay_window(Core::State::AppState& state) -> void;

// 隐藏叠加层窗口
export auto hide_overlay_window(Core::State::AppState& state) -> void;

// 检查叠加层窗口是否可见
export auto is_overlay_window_visible(const Core::State::AppState& state) -> bool;

// 更新叠加层窗口尺寸
export auto update_overlay_window_size(Core::State::AppState& state, int game_width, int game_height)
    -> std::expected<void, std::string>;

// 设置叠加层窗口位置
export auto position_overlay_window(Core::State::AppState& state) -> void;

// 销毁叠加层窗口
export auto destroy_overlay_window(Core::State::AppState& state) -> void;

// 获取叠加层窗口句柄
export auto get_overlay_window_handle(const Core::State::AppState& state) -> HWND;

// 恢复游戏窗口
export auto restore_game_window(Core::State::AppState& state, bool with_delay = false) -> void;

// 设置游戏窗口透明度
export auto set_game_window_transparency(Core::State::AppState& state, BYTE alpha) -> std::expected<void, std::string>;

}  // namespace Features::Overlay::Window
