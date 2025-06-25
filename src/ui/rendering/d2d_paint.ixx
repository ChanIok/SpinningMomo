module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.Rendering.D2DPaint;

import std;
import Core.State;
import Types.UI;
import UI.AppWindow.Rendering; // 为了使用现有的辅助函数

namespace UI::Rendering::D2DPaint {

// 内部函数声明
auto draw_background_d2d(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_title_bar_d2d(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_separators_d2d(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_items_d2d(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void;
auto draw_single_item_d2d(const Core::State::AppState& state, const Core::State::MenuItem& item,
                          const D2D1_RECT_F& item_rect, bool is_hovered) -> void;

// 主绘制函数（替换现有的paint_window）
export auto paint_window_d2d(const Core::State::AppState& state, const RECT& client_rect) -> void;

}  // namespace UI::Rendering::D2DPaint
