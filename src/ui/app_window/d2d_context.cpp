module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

module UI.AppWindow.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::AppWindow::D2DContext {

// 辅助函数：安全创建画刷
auto create_brush_safe(ID2D1RenderTarget* target, const D2D1_COLOR_F& color,
                       ID2D1SolidColorBrush** brush) -> bool {
  return SUCCEEDED(target->CreateSolidColorBrush(color, brush));
}

// 辅助函数：批量创建所有画刷
auto create_all_brushes_simple(Types::UI::D2DRenderState& d2d) -> bool {
  return create_brush_safe(d2d.render_target, Types::UI::D2DColors::WHITE, &d2d.white_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::TITLE_BAR, &d2d.title_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::SEPARATOR,
                           &d2d.separator_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::TEXT, &d2d.text_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::INDICATOR,
                           &d2d.indicator_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::HOVER, &d2d.hover_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::WHITE_SEMI,
                           &d2d.white_semi_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::TITLE_BAR_SEMI,
                           &d2d.title_semi_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::SEPARATOR_SEMI,
                           &d2d.separator_semi_brush) &&
         create_brush_safe(d2d.render_target, Types::UI::D2DColors::HOVER_SEMI,
                           &d2d.hover_semi_brush);
}

// 初始化Direct2D资源
auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& d2d = state.d2d_render;

  // 获取窗口大小
  RECT rc;
  GetClientRect(hwnd, &rc);
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;

  if (width <= 0 || height <= 0) {
    return false;
  }

  // 清理现有资源
  cleanup_d2d(state);

  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d.factory);
  if (FAILED(hr)) {
    return false;
  }

  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&d2d.write_factory));
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  // 创建内存DC和32位BGRA位图
  HDC screen_dc = GetDC(nullptr);
  d2d.memory_dc = CreateCompatibleDC(screen_dc);
  ReleaseDC(nullptr, screen_dc);

  if (!d2d.memory_dc) {
    cleanup_d2d(state);
    return false;
  }

  // 创建32位BGRA位图
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // 负值表示从上到下
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  d2d.dib_bitmap =
      CreateDIBSection(d2d.memory_dc, &bmi, DIB_RGB_COLORS, &d2d.bitmap_bits, nullptr, 0);
  if (!d2d.dib_bitmap) {
    cleanup_d2d(state);
    return false;
  }

  d2d.old_bitmap = SelectObject(d2d.memory_dc, d2d.dib_bitmap);
  d2d.bitmap_size = {width, height};

  // 创建DC渲染目标
  D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0,
      0,  // 使用默认DPI
      D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);

  hr = d2d.factory->CreateDCRenderTarget(&rtProps, &d2d.render_target);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  // 绑定DC渲染目标到内存DC
  RECT binding_rect = {0, 0, width, height};
  hr = d2d.render_target->BindDC(d2d.memory_dc, &binding_rect);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  // 创建所有画刷（使用简化的批量创建函数）
  if (!create_all_brushes_simple(d2d)) {
    cleanup_d2d(state);
    return false;
  }

  // 创建文本格式
  hr = d2d.write_factory->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                           13.0f, L"zh-CN", &d2d.text_format);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  d2d.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  d2d.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  d2d.is_initialized = true;
  return true;
}

// 清理Direct2D资源
auto cleanup_d2d(Core::State::AppState& state) -> void {
  auto& d2d = state.d2d_render;

  // 释放画刷
  if (d2d.white_brush) {
    d2d.white_brush->Release();
    d2d.white_brush = nullptr;
  }
  if (d2d.title_brush) {
    d2d.title_brush->Release();
    d2d.title_brush = nullptr;
  }
  if (d2d.separator_brush) {
    d2d.separator_brush->Release();
    d2d.separator_brush = nullptr;
  }
  if (d2d.text_brush) {
    d2d.text_brush->Release();
    d2d.text_brush = nullptr;
  }
  if (d2d.indicator_brush) {
    d2d.indicator_brush->Release();
    d2d.indicator_brush = nullptr;
  }
  if (d2d.hover_brush) {
    d2d.hover_brush->Release();
    d2d.hover_brush = nullptr;
  }

  // 释放半透明画刷
  if (d2d.white_semi_brush) {
    d2d.white_semi_brush->Release();
    d2d.white_semi_brush = nullptr;
  }
  if (d2d.title_semi_brush) {
    d2d.title_semi_brush->Release();
    d2d.title_semi_brush = nullptr;
  }
  if (d2d.separator_semi_brush) {
    d2d.separator_semi_brush->Release();
    d2d.separator_semi_brush = nullptr;
  }
  if (d2d.hover_semi_brush) {
    d2d.hover_semi_brush->Release();
    d2d.hover_semi_brush = nullptr;
  }

  // 释放文本格式
  if (d2d.text_format) {
    d2d.text_format->Release();
    d2d.text_format = nullptr;
  }

  // 释放DirectWrite工厂
  if (d2d.write_factory) {
    d2d.write_factory->Release();
    d2d.write_factory = nullptr;
  }

  // 释放内存DC和位图
  if (d2d.memory_dc) {
    DeleteDC(d2d.memory_dc);
    d2d.memory_dc = nullptr;
  }
  if (d2d.dib_bitmap) {
    DeleteObject(d2d.dib_bitmap);
    d2d.dib_bitmap = nullptr;
  }
  if (d2d.old_bitmap) {
    SelectObject(d2d.memory_dc, d2d.old_bitmap);
    d2d.old_bitmap = nullptr;
  }

  // 释放DC渲染目标
  if (d2d.render_target) {
    d2d.render_target->Release();
    d2d.render_target = nullptr;
  }

  // 释放D2D工厂
  if (d2d.factory) {
    d2d.factory->Release();
    d2d.factory = nullptr;
  }

  d2d.is_initialized = false;
  d2d.needs_resize = false;
}

// 调整渲染目标大小
auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& d2d = state.d2d_render;

  if (!d2d.is_initialized || !d2d.render_target) {
    return false;
  }

  // 对于DC渲染目标，我们需要重新创建位图和重新绑定
  if (d2d.bitmap_size.cx == new_size.cx && d2d.bitmap_size.cy == new_size.cy) {
    // 大小没有改变，无需重新创建
    d2d.needs_resize = false;
    return true;
  }

  // 释放旧的位图
  if (d2d.old_bitmap) {
    SelectObject(d2d.memory_dc, d2d.old_bitmap);
    d2d.old_bitmap = nullptr;
  }
  if (d2d.dib_bitmap) {
    DeleteObject(d2d.dib_bitmap);
    d2d.dib_bitmap = nullptr;
    d2d.bitmap_bits = nullptr;
  }

  // 创建新的32位BGRA位图
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = new_size.cx;
  bmi.bmiHeader.biHeight = -new_size.cy;  // 负值表示从上到下
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  d2d.dib_bitmap =
      CreateDIBSection(d2d.memory_dc, &bmi, DIB_RGB_COLORS, &d2d.bitmap_bits, nullptr, 0);
  if (!d2d.dib_bitmap) {
    return false;
  }

  d2d.old_bitmap = SelectObject(d2d.memory_dc, d2d.dib_bitmap);
  d2d.bitmap_size = new_size;

  // 重新绑定DC渲染目标
  RECT binding_rect = {0, 0, new_size.cx, new_size.cy};
  HRESULT hr = d2d.render_target->BindDC(d2d.memory_dc, &binding_rect);

  if (SUCCEEDED(hr)) {
    d2d.needs_resize = false;
    return true;
  }

  return false;
}

// 更新文本格式（DPI变化时）
auto update_text_format_if_needed(Core::State::AppState& state) -> bool {
  auto& d2d = state.d2d_render;
  auto& layout = state.app_window.layout;

  // 如果不需要更新或工厂不可用，直接返回成功
  if (!d2d.needs_font_update || !d2d.write_factory) {
    return true;
  }

  // 释放旧的文本格式
  if (d2d.text_format) {
    d2d.text_format->Release();
    d2d.text_format = nullptr;
  }

  // 创建新的文本格式（使用DPI缩放后的字体大小）
  HRESULT hr =
      d2d.write_factory->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                          layout.font_size,  // 使用DPI缩放后的字体大小
                                          L"zh-CN", &d2d.text_format);

  if (SUCCEEDED(hr) && d2d.text_format) {
    // 设置文本对齐方式
    hr = d2d.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (SUCCEEDED(hr)) {
      hr = d2d.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (SUCCEEDED(hr)) {
      d2d.needs_font_update = false;
      return true;
    }
  }

  // 创建失败，清理资源
  if (d2d.text_format) {
    d2d.text_format->Release();
    d2d.text_format = nullptr;
  }

  return false;
}

}  // namespace UI::AppWindow::D2DContext
