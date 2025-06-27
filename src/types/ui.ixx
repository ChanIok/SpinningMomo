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
  ID2D1DCRenderTarget* render_target = nullptr;
  IDWriteFactory* write_factory = nullptr;

  // 内存DC和位图资源
  HDC memory_dc = nullptr;
  HBITMAP dib_bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  void* bitmap_bits = nullptr;
  SIZE bitmap_size = {0, 0};

  // 缓存的画刷（简单的固定数组，避免动态分配）
  // 不透明画刷（用于文字和指示器）
  ID2D1SolidColorBrush* white_brush = nullptr;
  ID2D1SolidColorBrush* title_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;

  // 半透明画刷（用于背景和装饰元素）
  ID2D1SolidColorBrush* white_semi_brush = nullptr;
  ID2D1SolidColorBrush* title_semi_brush = nullptr;
  ID2D1SolidColorBrush* separator_semi_brush = nullptr;
  ID2D1SolidColorBrush* hover_semi_brush = nullptr;

  // 文本格式
  IDWriteTextFormat* text_format = nullptr;

  // 状态标志
  bool is_initialized = false;
  bool is_rendering = false;
  bool needs_resize = false;
  bool needs_font_update = false;
};

// One Dark Pro风格暗色主题颜色常量
struct D2DColors {
  // === One Dark Pro主色调 ===
  static constexpr D2D1_COLOR_F WHITE = {0.16f, 0.17f, 0.21f, 1.0f};      // #282C34 主背景
  static constexpr D2D1_COLOR_F TITLE_BAR = {0.13f, 0.14f, 0.17f, 1.0f};  // #21252B 标题栏
  static constexpr D2D1_COLOR_F SEPARATOR = {0.20f, 0.22f, 0.27f, 1.0f};  // #333842 分隔线
  static constexpr D2D1_COLOR_F TEXT = {0.67f, 0.71f, 0.78f, 1.0f};       // #ABB2BF 主文字
  static constexpr D2D1_COLOR_F INDICATOR = {0.38f, 0.68f, 0.84f, 1.0f};  // #61AFEF 青色指示器
  static constexpr D2D1_COLOR_F HOVER = {0.19f, 0.20f, 0.25f, 1.0f};      // #2F3240 悬停背景

  // === 半透明版本（用于分层窗口） ===
  static constexpr D2D1_COLOR_F WHITE_SEMI = {0.16f, 0.17f, 0.21f, 0.85f};     // 85%透明度
  static constexpr D2D1_COLOR_F TITLE_BAR_SEMI = {0.13f, 0.14f, 0.17f, 0.9f};  // 90%透明度
  static constexpr D2D1_COLOR_F SEPARATOR_SEMI = {0.20f, 0.22f, 0.27f, 0.7f};  // 70%透明度
  static constexpr D2D1_COLOR_F HOVER_SEMI = {0.19f, 0.20f, 0.25f, 0.8f};      // 80%透明度
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
