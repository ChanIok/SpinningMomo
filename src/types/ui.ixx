module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module Types.UI;

import std;

export namespace Types::UI {

// Direct2D渲染状态
struct D2DRenderState {
  // Direct2D资源句柄（使用原始指针，符合POD风格）
  ID2D1Factory* factory = nullptr;
  ID2D1HwndRenderTarget* render_target = nullptr;
  IDWriteFactory* write_factory = nullptr;

  // 缓存的画刷（简单的固定数组，避免动态分配）
  ID2D1SolidColorBrush* white_brush = nullptr;
  ID2D1SolidColorBrush* title_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;

  // 文本格式
  IDWriteTextFormat* text_format = nullptr;

  // 状态标志
  bool is_initialized = false;
  bool needs_resize = false;
};

// 颜色常量（POD结构）
struct D2DColors {
  static constexpr D2D1_COLOR_F WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
  static constexpr D2D1_COLOR_F TITLE_BAR = {240.0f / 255, 240.0f / 255, 240.0f / 255, 1.0f};
  static constexpr D2D1_COLOR_F SEPARATOR = {229.0f / 255, 229.0f / 255, 229.0f / 255, 1.0f};
  static constexpr D2D1_COLOR_F TEXT = {51.0f / 255, 51.0f / 255, 51.0f / 255, 1.0f};
  static constexpr D2D1_COLOR_F INDICATOR = {255.0f / 255, 160.0f / 255, 80.0f / 255, 1.0f};
  static constexpr D2D1_COLOR_F HOVER = {242.0f / 255, 242.0f / 255, 242.0f / 255, 1.0f};
};

// 辅助函数：将RECT转换为D2D1_RECT_F
export auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

// 辅助函数：创建D2D矩形
export auto make_d2d_rect(float left, float top, float right, float bottom) -> D2D1_RECT_F {
  return D2D1::RectF(left, top, right, bottom);
}

}  // namespace Types::UI
