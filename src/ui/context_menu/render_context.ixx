module;

export module UI.ContextMenu.RenderContext;

import std;
import Core.State;
import UI.ContextMenu.State;
import <windows.h>;

namespace UI::ContextMenu::RenderContext {

export auto initialize_text_format(Core::State::AppState& app_state) -> bool;

// 主菜单D2D资源管理
export auto initialize_context_menu(Core::State::AppState& app_state, HWND hwnd) -> bool;
export auto cleanup_context_menu(Core::State::AppState& app_state) -> void;

// 子菜单D2D资源管理
export auto initialize_submenu(Core::State::AppState& app_state, HWND hwnd) -> bool;
export auto cleanup_submenu(Core::State::AppState& app_state) -> void;

// 调整渲染目标大小
export auto resize_context_menu(Core::State::AppState& app_state, const SIZE& new_size) -> bool;
export auto resize_submenu(Core::State::AppState& app_state, const SIZE& new_size) -> bool;

}  // namespace UI::ContextMenu::RenderContext
