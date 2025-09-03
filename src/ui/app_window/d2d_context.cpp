module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <string>

module UI.AppWindow.D2DContext;

import Core.State;
import UI.AppWindow.State;
import UI.AppWindow.Types;
import Features.Settings.State;

namespace UI::AppWindow::D2DContext {

// 辅助函数：从包含透明度的十六进制颜色字符串创建 D2D1_COLOR_F
auto hex_with_alpha_to_color_f(const std::string& hex_color) -> D2D1_COLOR_F {
  // 支持 #RRGGBBAA 格式，如果只有6位则默认透明度为FF
  std::string color_str = hex_color;
  if (color_str.starts_with("#")) {
    color_str = color_str.substr(1);
  }

  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

  if (color_str.length() == 8) {  // #RRGGBBAA
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
    a = std::stoi(color_str.substr(6, 2), nullptr, 16) / 255.0f;
  } else if (color_str.length() == 6) {  // #RRGGBB
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
    a = 1.0f;
  }

  return D2D1::ColorF(r, g, b, a);
}

// 辅助函数：从十六进制颜色字符串创建画刷
auto create_brush_from_hex(ID2D1RenderTarget* target, const std::string& hex_color,
                           ID2D1SolidColorBrush** brush) -> bool {
  return SUCCEEDED(target->CreateSolidColorBrush(hex_with_alpha_to_color_f(hex_color), brush));
}

// 辅助函数：批量创建所有画刷
auto create_all_brushes_simple(Core::State::AppState& state, UI::AppWindow::RenderContext& d2d)
    -> bool {
  const auto& colors = state.settings->config.ui.app_window_colors;

  return create_brush_from_hex(d2d.render_target, colors.background, &d2d.background_brush) &&
         create_brush_from_hex(d2d.render_target, colors.separator, &d2d.separator_brush) &&
         create_brush_from_hex(d2d.render_target, colors.text, &d2d.text_brush) &&
         create_brush_from_hex(d2d.render_target, colors.indicator, &d2d.indicator_brush) &&
         create_brush_from_hex(d2d.render_target, colors.title_bar, &d2d.title_brush) &&
         create_brush_from_hex(d2d.render_target, colors.hover, &d2d.hover_brush);
}

// 辅助函数：测量文本宽度
auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                        IDWriteFactory7* write_factory) -> float {
  if (text.empty() || !text_format || !write_factory) {
    return 0.0f;
  }

  // 创建文本布局
  IDWriteTextLayout* text_layout = nullptr;
  HRESULT hr =
      write_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()), text_format,
                                      10000.0f,  // 宽度（足够大以避免换行）
                                      10000.0f,  // 高度
                                      &text_layout);

  if (FAILED(hr) || !text_layout) {
    return 0.0f;
  }

  // 获取文本布局的度量信息
  DWRITE_TEXT_METRICS metrics = {};
  hr = text_layout->GetMetrics(&metrics);
  text_layout->Release();

  if (FAILED(hr)) {
    return 0.0f;
  }

  return metrics.width;
}

// 更新所有画刷颜色
auto update_all_brush_colors(Core::State::AppState& state) -> void {
  const auto& colors = state.settings->config.ui.app_window_colors;
  auto& d2d = state.app_window->d2d_context;

  // 更新画刷颜色
  if (d2d.background_brush) {
    d2d.background_brush->SetColor(hex_with_alpha_to_color_f(colors.background));
  }
  if (d2d.title_brush) {
    d2d.title_brush->SetColor(hex_with_alpha_to_color_f(colors.title_bar));
  }
  if (d2d.separator_brush) {
    d2d.separator_brush->SetColor(hex_with_alpha_to_color_f(colors.separator));
  }
  if (d2d.text_brush) {
    d2d.text_brush->SetColor(hex_with_alpha_to_color_f(colors.text));
  }
  if (d2d.indicator_brush) {
    d2d.indicator_brush->SetColor(hex_with_alpha_to_color_f(colors.indicator));
  }
  if (d2d.hover_brush) {
    d2d.hover_brush->SetColor(hex_with_alpha_to_color_f(colors.hover));
  }
}

// 创建具有指定字体大小的文本格式
auto create_text_format_with_size(IDWriteFactory7* write_factory, float font_size)
    -> IDWriteTextFormat* {
  if (!write_factory) {
    return nullptr;
  }

  IDWriteTextFormat* text_format = nullptr;
  HRESULT hr = write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-CN", &text_format);

  if (FAILED(hr) || !text_format) {
    return nullptr;
  }

  // 设置文本对齐方式
  text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  return text_format;
}

// 初始化Direct2D资源
auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& d2d = state.app_window->d2d_context;

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

  // 创建Direct2D 1.3工厂
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7),
                                 nullptr, reinterpret_cast<void**>(&d2d.factory));
  if (FAILED(hr)) {
    return false;
  }

  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
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

  // 尝试获取设备上下文接口（Direct2D 1.3功能）
  hr = d2d.render_target->QueryInterface(__uuidof(ID2D1DeviceContext6),
                                         reinterpret_cast<void**>(&d2d.device_context));
  // 注意：DC渲染目标可能不支持设备上下文，这是正常的
  // 我们将在绘制时检查是否可用

  // 创建所有画刷（使用简化的批量创建函数）
  if (!create_all_brushes_simple(state, d2d)) {
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
  auto& d2d = state.app_window->d2d_context;

  // 释放画刷
  if (d2d.background_brush) {
    d2d.background_brush->Release();
    d2d.background_brush = nullptr;
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

  // 释放设备上下文
  if (d2d.device_context) {
    d2d.device_context->Release();
    d2d.device_context = nullptr;
  }

  // 释放DC渲染目标
  if (d2d.render_target) {
    d2d.render_target->Release();
    d2d.render_target = nullptr;
  }

  // 释放D2D 1.3工厂
  if (d2d.factory) {
    d2d.factory->Release();
    d2d.factory = nullptr;
  }

  d2d.is_initialized = false;
  d2d.needs_resize = false;
}

// 调整渲染目标大小
auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& d2d = state.app_window->d2d_context;

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
  auto& d2d = state.app_window->d2d_context;
  auto& layout = state.app_window->layout;

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
