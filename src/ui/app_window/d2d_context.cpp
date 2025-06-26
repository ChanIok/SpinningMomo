module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

module UI.AppWindow.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::AppWindow::D2DContext {

// 初始化Direct2D资源
auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& d2d = state.d2d_render;

  // 如果已经初始化，直接返回成功
  if (d2d.is_initialized) {
    return true;
  }

  // 创建D2D工厂
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d.factory);
  if (FAILED(hr)) {
    return false;
  }

  // 获取窗口客户区大小
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 创建渲染目标
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  hr = d2d.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                           D2D1::HwndRenderTargetProperties(hwnd, size),
                                           &d2d.render_target);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  // 创建DirectWrite工厂
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&d2d.write_factory));
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  // 创建文本格式
  HRESULT hr2 = d2d.write_factory->CreateTextFormat(
      L"微软雅黑",                                            // 字体名称
      nullptr,                                                // 字体集合
      DWRITE_FONT_WEIGHT_NORMAL,                              // 字体粗细
      DWRITE_FONT_STYLE_NORMAL,                               // 字体样式
      DWRITE_FONT_STRETCH_NORMAL,                             // 字体拉伸
      static_cast<float>(state.app_window.layout.font_size),  // 字体大小
      L"zh-cn",                                               // 区域设置
      &d2d.text_format);
  if (FAILED(hr2)) {
    cleanup_d2d(state);
    return false;
  }

  // 设置文本对齐方式
  d2d.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  d2d.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  // 创建画刷
  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::WHITE, &d2d.white_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::TITLE_BAR, &d2d.title_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::SEPARATOR,
                                                &d2d.separator_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::TEXT, &d2d.text_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::INDICATOR,
                                                &d2d.indicator_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = d2d.render_target->CreateSolidColorBrush(Types::UI::D2DColors::HOVER, &d2d.hover_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

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

  // 释放渲染目标
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

  D2D1_SIZE_U size = D2D1::SizeU(new_size.cx, new_size.cy);
  HRESULT hr = d2d.render_target->Resize(size);

  if (SUCCEEDED(hr)) {
    d2d.needs_resize = false;
    return true;
  }

  return false;
}

}  // namespace UI::AppWindow::D2DContext
