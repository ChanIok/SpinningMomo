module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.AppWindow.Painter;

import std;
import Core.State;
import Types.UI;
import UI.AppWindow.State;

namespace UI::AppWindow::Painter {

// 内部函数声明
auto draw_app_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_title_bar(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_separators(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_app_single_item(const Core::State::AppState& state, const UI::AppWindow::MenuItem& item,
                          const D2D1_RECT_F& item_rect, bool is_hovered) -> void;

// 主绘制函数（替换现有的paint_window）
export auto paint_app_window(const Core::State::AppState& state, const RECT& client_rect) -> void;

}  // namespace UI::AppWindow::Painter