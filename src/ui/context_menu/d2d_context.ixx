module;

export module UI.ContextMenu.D2DContext;

import std;
import Core.State;
import UI.ContextMenu.State;
import <windows.h>;

export namespace UI::ContextMenu::D2DContext {

using State::ContextMenuState;

auto initialize_text_format(Core::State::AppState& app_state) -> bool;

// 主菜单D2D资源管理
auto initialize_context_menu(Core::State::AppState& app_state, HWND hwnd) -> bool;
auto cleanup_context_menu(Core::State::AppState& app_state) -> void;

// 子菜单D2D资源管理
auto initialize_submenu(Core::State::AppState& app_state, HWND hwnd) -> bool;
auto cleanup_submenu(Core::State::AppState& app_state) -> void;

// 调整渲染目标大小
auto resize_context_menu(Core::State::AppState& app_state, const SIZE& new_size) -> bool;
auto resize_submenu(Core::State::AppState& app_state, const SIZE& new_size) -> bool;

}  // namespace UI::ContextMenu::D2DContext
