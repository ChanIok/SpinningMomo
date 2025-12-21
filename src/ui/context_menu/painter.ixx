module;

#include <d2d1.h>
#include <windows.h>

export module UI.ContextMenu.Painter;

import std;
import Core.State;
import UI.FloatingWindow.Types;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;

export namespace UI::ContextMenu::Painter {

using State::ContextMenuState;

// 主菜单绘制
auto paint_context_menu(Core::State::AppState& app_state, const RECT& client_rect) -> void;

// 子菜单绘制
auto paint_submenu(Core::State::AppState& app_state, const RECT& client_rect) -> void;

// 内部绘制函数
auto draw_menu_background(Core::State::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_menu_items(Core::State::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_single_menu_item(Core::State::AppState& app_state, const Types::MenuItem& item,
                           const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_separator(Core::State::AppState& app_state, const D2D1_RECT_F& separator_rect) -> void;

auto draw_submenu_background(Core::State::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_items(Core::State::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_single_item(Core::State::AppState& app_state, const Types::MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_submenu_separator(Core::State::AppState& app_state, const D2D1_RECT_F& separator_rect)
    -> void;

}  // namespace UI::ContextMenu::Painter
