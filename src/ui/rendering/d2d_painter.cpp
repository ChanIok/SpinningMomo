module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

module UI.Rendering.D2DPainter;

import std;
import Types.UI;

namespace UI::Rendering::D2DPainter {

// 通用绘制会话管理
auto begin_d2d_draw(const Types::UI::D2DRenderState& d2d_state) -> bool {
  if (!d2d_state.is_initialized || !d2d_state.render_target) {
    return false;
  }

  if (d2d_state.needs_resize) {
    return false;
  }

  d2d_state.render_target->BeginDraw();
  return true;
}

auto end_d2d_draw(Types::UI::D2DRenderState& d2d_state) -> bool {
  HRESULT hr = d2d_state.render_target->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    d2d_state.needs_resize = true;
    return false;
  }
  return SUCCEEDED(hr);
}

// 通用绘制函数
auto fill_rectangle(const Types::UI::D2DRenderState& d2d_state, const D2D1_RECT_F& rect,
                    ID2D1SolidColorBrush* brush) -> void {
  if (d2d_state.render_target && brush) {
    d2d_state.render_target->FillRectangle(rect, brush);
  }
}

auto draw_text(const Types::UI::D2DRenderState& d2d_state, const std::wstring& text,
               const D2D1_RECT_F& rect, ID2D1SolidColorBrush* brush) -> void {
  if (d2d_state.render_target && d2d_state.text_format && brush) {
    d2d_state.render_target->DrawText(text.c_str(), static_cast<UINT32>(text.length()),
                                      d2d_state.text_format, rect, brush);
  }
}

}  // namespace UI::Rendering::D2DPainter
