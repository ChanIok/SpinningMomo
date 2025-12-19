module;

#include <d2d1_3.h>
#include <windows.h>

export module UI.AppWindow.Painter;

import std;
import Core.State;
import UI.AppWindow.Types;

namespace UI::AppWindow::Painter {

// 内部函数声明
auto draw_app_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_title_bar(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_separators(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_single_item(const Core::State::AppState& state, const UI::AppWindow::MenuItem& item,
                          const D2D1_RECT_F& item_rect, bool is_hovered) -> void;
auto draw_scroll_indicator(const Core::State::AppState& state, const D2D1_RECT_F& column_rect,
                           size_t total_items, size_t scroll_offset, bool is_hovered) -> void;

// 分层窗口更新函数
export auto update_layered_window(const Core::State::AppState& state, HWND hwnd) -> void;

// 主绘制函数
export auto paint_app_window(Core::State::AppState& state, HWND hwnd, const RECT& client_rect)
    -> void;

}  // namespace UI::AppWindow::Painter