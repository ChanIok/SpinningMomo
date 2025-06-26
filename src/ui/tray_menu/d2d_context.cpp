module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

module UI.TrayMenu.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::TrayMenu::D2DContext {

// 初始化托盘菜单的Direct2D资源
auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& tray_menu = state.tray_menu;
  const auto& d2d = state.d2d_render;

  // 如果已经初始化，直接返回成功
  if (tray_menu.render_target_initialized) {
    return true;
  }

  // 检查全局D2D工厂是否已初始化
  if (!d2d.is_initialized || !d2d.factory) {
    return false;
  }

  // 获取托盘菜单窗口的客户区大小
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 创建托盘菜单专用的渲染目标
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                   D2D1::HwndRenderTargetProperties(hwnd, size),
                                                   &tray_menu.render_target);

  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  tray_menu.render_target_initialized = true;

  // 创建托盘菜单专用画刷
  hr = tray_menu.render_target->CreateSolidColorBrush(Types::UI::D2DColors::WHITE,
                                                      &tray_menu.white_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = tray_menu.render_target->CreateSolidColorBrush(Types::UI::D2DColors::TEXT,
                                                      &tray_menu.text_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = tray_menu.render_target->CreateSolidColorBrush(Types::UI::D2DColors::SEPARATOR,
                                                      &tray_menu.separator_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = tray_menu.render_target->CreateSolidColorBrush(Types::UI::D2DColors::HOVER,
                                                      &tray_menu.hover_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  hr = tray_menu.render_target->CreateSolidColorBrush(Types::UI::D2DColors::INDICATOR,
                                                      &tray_menu.indicator_brush);
  if (FAILED(hr)) {
    cleanup_d2d(state);
    return false;
  }

  tray_menu.brushes_initialized = true;
  return true;
}

// 清理托盘菜单的Direct2D资源
auto cleanup_d2d(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  // 释放画刷
  if (tray_menu.white_brush) {
    tray_menu.white_brush->Release();
    tray_menu.white_brush = nullptr;
  }
  if (tray_menu.text_brush) {
    tray_menu.text_brush->Release();
    tray_menu.text_brush = nullptr;
  }
  if (tray_menu.separator_brush) {
    tray_menu.separator_brush->Release();
    tray_menu.separator_brush = nullptr;
  }
  if (tray_menu.hover_brush) {
    tray_menu.hover_brush->Release();
    tray_menu.hover_brush = nullptr;
  }
  if (tray_menu.indicator_brush) {
    tray_menu.indicator_brush->Release();
    tray_menu.indicator_brush = nullptr;
  }

  // 释放渲染目标
  if (tray_menu.render_target) {
    tray_menu.render_target->Release();
    tray_menu.render_target = nullptr;
  }

  tray_menu.render_target_initialized = false;
  tray_menu.brushes_initialized = false;
}

// 调整托盘菜单渲染目标大小
auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& tray_menu = state.tray_menu;

  if (!tray_menu.render_target_initialized || !tray_menu.render_target) {
    return false;
  }

  D2D1_SIZE_U size = D2D1::SizeU(new_size.cx, new_size.cy);
  HRESULT hr = tray_menu.render_target->Resize(size);

  return SUCCEEDED(hr);
}

}  // namespace UI::TrayMenu::D2DContext
