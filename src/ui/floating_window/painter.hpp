#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/types.hpp"

namespace ui::floating_window::painter {

// 内部函数声明
auto draw_background(const core::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_title_bar(const core::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_separators(const core::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_items(core::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_single_item(core::AppState& state, const ui::floating_window::MenuItem& item,
                      const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_scroll_indicator(const core::AppState& state, const D2D1_RECT_F& column_rect,
                           size_t total_items, size_t scroll_offset, bool is_hovered,
                           bool is_last_column) -> void;

// 主绘制函数
auto paint(core::AppState& state, HWND hwnd, const RECT& client_rect) -> void;

}  // namespace ui::floating_window::painter
