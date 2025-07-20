module;

#include <windows.h>

export module UI.TrayMenu.D2DContext;

import std;
import Core.State;
import UI.AppWindow.Types;
import UI.AppWindow.State;

export namespace UI::TrayMenu::D2DContext {

// 主菜单D2D资源管理
auto initialize_main_menu(Core::State::AppState& state, HWND hwnd) -> bool;
auto cleanup_main_menu(Core::State::AppState& state) -> void;

// 子菜单D2D资源管理
auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool;
auto cleanup_submenu(Core::State::AppState& state) -> void;

// 调整渲染目标大小（保留用于窗口大小变化）
auto resize_main_menu(Core::State::AppState& state, const SIZE& new_size) -> bool;
auto resize_submenu(Core::State::AppState& state, const SIZE& new_size) -> bool;

}  // namespace UI::TrayMenu::D2DContext
