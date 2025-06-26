module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.TrayMenu.Painter;

import std;
import Core.State;
import Types.UI;
import UI.TrayMenu.State;

namespace UI::TrayMenu::Painter {

// 主绘制函数
export auto paint_tray_menu(const Core::State::AppState& state, const RECT& client_rect) -> void;

// 内部绘制函数
auto draw_menu_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_menu_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_single_menu_item(const Core::State::AppState& state,
                           const UI::TrayMenu::State::MenuItem& item, const D2D1_RECT_F& item_rect,
                           bool is_hovered) -> void;
auto draw_separator(const Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void;

}  // namespace UI::TrayMenu::Painter
