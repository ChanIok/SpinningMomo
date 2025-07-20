module;

#include <d2d1.h>
#include <windows.h>

export module UI.TrayMenu.Painter;

import std;
import Core.State;
import UI.AppWindow.Types;
import UI.TrayMenu.State;

export namespace UI::TrayMenu::Painter {

// 主菜单绘制
auto paint_tray_menu(const Core::State::AppState& state, const RECT& client_rect) -> void;

// 子菜单绘制
auto paint_submenu(const Core::State::AppState& state, const RECT& client_rect) -> void;

auto draw_menu_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_menu_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_single_menu_item(const Core::State::AppState& state,
                           const UI::TrayMenu::State::MenuItem& item, const D2D1_RECT_F& item_rect,
                           bool is_hovered) -> void;
auto draw_separator(const Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void;

auto draw_submenu_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_single_item(const Core::State::AppState& state,
                              const UI::TrayMenu::State::MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_submenu_separator(const Core::State::AppState& state, const D2D1_RECT_F& separator_rect)
    -> void;

}  // namespace UI::TrayMenu::Painter
