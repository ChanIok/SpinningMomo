module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.Rendering.D2DPainter;

import std;
import Types.UI;

namespace UI::Rendering::D2DPainter {

// 绘制会话管理
export auto begin_d2d_draw(const Types::UI::D2DRenderState& d2d_state) -> bool;
export auto end_d2d_draw(Types::UI::D2DRenderState& d2d_state) -> bool;

// 通用绘制函数
export auto fill_rectangle(const Types::UI::D2DRenderState& d2d_state, const D2D1_RECT_F& rect,
                           ID2D1SolidColorBrush* brush) -> void;
export auto draw_text(const Types::UI::D2DRenderState& d2d_state, const std::wstring& text,
                      const D2D1_RECT_F& rect, ID2D1SolidColorBrush* brush) -> void;

}  // namespace UI::Rendering::D2DPainter
