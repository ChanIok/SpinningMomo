#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/d2d1.hpp"

#include "core/state/app_state.hpp"
#include "ui/context_menu/state.hpp"
#include "ui/context_menu/types.hpp"
#include "ui/floating_window/types.hpp"

namespace ui::context_menu::painter {

// 主菜单绘制
auto paint_context_menu(core::AppState& app_state, const RECT& client_rect) -> void;

// 子菜单绘制
auto paint_submenu(core::AppState& app_state, const RECT& client_rect) -> void;

// 内部绘制函数
auto draw_menu_background(core::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_menu_items(core::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_single_menu_item(core::AppState& app_state, const MenuItem& item,
                           const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_separator(core::AppState& app_state, const D2D1_RECT_F& separator_rect) -> void;

auto draw_submenu_background(core::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_items(core::AppState& app_state, const D2D1_RECT_F& rect) -> void;
auto draw_submenu_single_item(core::AppState& app_state, const MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_submenu_separator(core::AppState& app_state, const D2D1_RECT_F& separator_rect) -> void;

}  // namespace ui::context_menu::painter
