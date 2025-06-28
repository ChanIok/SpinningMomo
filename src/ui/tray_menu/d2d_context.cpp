module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <iostream>

module UI.TrayMenu.D2DContext;

import std;
import Core.State;
import Types.UI;
import Utils.Logger;

namespace {

// 创建画刷的通用函数
auto create_brushes_for_target(ID2D1HwndRenderTarget* target, ID2D1SolidColorBrush** white_brush,
                               ID2D1SolidColorBrush** text_brush,
                               ID2D1SolidColorBrush** separator_brush,
                               ID2D1SolidColorBrush** hover_brush,
                               ID2D1SolidColorBrush** indicator_brush) -> bool {
  HRESULT hr;

  hr = target->CreateSolidColorBrush(Types::UI::D2DColors::WHITE, white_brush);
  if (FAILED(hr)) return false;

  hr = target->CreateSolidColorBrush(Types::UI::D2DColors::TEXT, text_brush);
  if (FAILED(hr)) return false;

  hr = target->CreateSolidColorBrush(Types::UI::D2DColors::SEPARATOR, separator_brush);
  if (FAILED(hr)) return false;

  hr = target->CreateSolidColorBrush(Types::UI::D2DColors::HOVER, hover_brush);
  if (FAILED(hr)) return false;

  hr = target->CreateSolidColorBrush(Types::UI::D2DColors::INDICATOR, indicator_brush);
  if (FAILED(hr)) return false;

  return true;
}

// 释放画刷的通用函数
auto release_brushes(ID2D1SolidColorBrush** white_brush, ID2D1SolidColorBrush** text_brush,
                     ID2D1SolidColorBrush** separator_brush, ID2D1SolidColorBrush** hover_brush,
                     ID2D1SolidColorBrush** indicator_brush) -> void {
  if (*white_brush) {
    (*white_brush)->Release();
    *white_brush = nullptr;
  }
  if (*text_brush) {
    (*text_brush)->Release();
    *text_brush = nullptr;
  }
  if (*separator_brush) {
    (*separator_brush)->Release();
    *separator_brush = nullptr;
  }
  if (*hover_brush) {
    (*hover_brush)->Release();
    *hover_brush = nullptr;
  }
  if (*indicator_brush) {
    (*indicator_brush)->Release();
    *indicator_brush = nullptr;
  }
}

}  // namespace

namespace UI::TrayMenu::D2DContext {

// 初始化主菜单D2D资源
auto initialize_main_menu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& tray_menu = state.tray_menu;
  const auto& d2d = state.d2d_render;

  // 如果已经初始化，直接返回成功
  if (tray_menu.main_menu_d2d_ready) {
    return true;
  }

  // 检查全局D2D工厂是否已初始化
  if (!d2d.is_initialized || !d2d.factory) {
    return false;
  }

  // 获取窗口的客户区大小
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 创建主菜单渲染目标
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                   D2D1::HwndRenderTargetProperties(hwnd, size),
                                                   &tray_menu.render_target);

  if (FAILED(hr)) {
    cleanup_main_menu(state);
    return false;
  }

  // 创建主菜单画刷
  if (!create_brushes_for_target(tray_menu.render_target, &tray_menu.white_brush,
                                 &tray_menu.text_brush, &tray_menu.separator_brush,
                                 &tray_menu.hover_brush, &tray_menu.indicator_brush)) {
    cleanup_main_menu(state);
    return false;
  }

  tray_menu.main_menu_d2d_ready = true;
  return true;
}

// 清理主菜单D2D资源
auto cleanup_main_menu(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  // 释放主菜单画刷
  release_brushes(&tray_menu.white_brush, &tray_menu.text_brush, &tray_menu.separator_brush,
                  &tray_menu.hover_brush, &tray_menu.indicator_brush);

  // 释放主菜单渲染目标
  if (tray_menu.render_target) {
    tray_menu.render_target->Release();
    tray_menu.render_target = nullptr;
  }

  tray_menu.main_menu_d2d_ready = false;
}

// 初始化子菜单D2D资源
auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& tray_menu = state.tray_menu;
  const auto& d2d = state.d2d_render;

  // 如果已经初始化，直接返回成功
  if (tray_menu.submenu_d2d_ready) {
    return true;
  }

  // 检查全局D2D工厂是否已初始化
  if (!d2d.is_initialized || !d2d.factory) {
    Logger().error("Global D2D not initialized or factory is null");
    return false;
  }

  // 获取窗口的客户区大小
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 创建子菜单渲染目标
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                   D2D1::HwndRenderTargetProperties(hwnd, size),
                                                   &tray_menu.submenu_render_target);

  if (FAILED(hr)) {
    Logger().error("Failed to create submenu render target, HRESULT: 0x{:X}", hr);
    cleanup_submenu(state);
    return false;
  }

  // 创建子菜单画刷
  if (!create_brushes_for_target(tray_menu.submenu_render_target, &tray_menu.submenu_white_brush,
                                 &tray_menu.submenu_text_brush, &tray_menu.submenu_separator_brush,
                                 &tray_menu.submenu_hover_brush,
                                 &tray_menu.submenu_indicator_brush)) {
    Logger().error("Failed to create submenu brushes");
    cleanup_submenu(state);
    return false;
  }

  tray_menu.submenu_d2d_ready = true;
  return true;
}

// 清理子菜单D2D资源
auto cleanup_submenu(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  // 释放子菜单画刷
  release_brushes(&tray_menu.submenu_white_brush, &tray_menu.submenu_text_brush,
                  &tray_menu.submenu_separator_brush, &tray_menu.submenu_hover_brush,
                  &tray_menu.submenu_indicator_brush);

  // 释放子菜单渲染目标
  if (tray_menu.submenu_render_target) {
    tray_menu.submenu_render_target->Release();
    tray_menu.submenu_render_target = nullptr;
  }

  tray_menu.submenu_d2d_ready = false;
}

// 调整主菜单渲染目标大小
auto resize_main_menu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& tray_menu = state.tray_menu;

  if (!tray_menu.main_menu_d2d_ready || !tray_menu.render_target) {
    return false;
  }

  D2D1_SIZE_U size = D2D1::SizeU(new_size.cx, new_size.cy);
  HRESULT hr = tray_menu.render_target->Resize(size);
  return SUCCEEDED(hr);
}

// 调整子菜单渲染目标大小
auto resize_submenu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& tray_menu = state.tray_menu;

  if (!tray_menu.submenu_d2d_ready || !tray_menu.submenu_render_target) {
    return false;
  }

  D2D1_SIZE_U size = D2D1::SizeU(new_size.cx, new_size.cy);
  HRESULT hr = tray_menu.submenu_render_target->Resize(size);
  return SUCCEEDED(hr);
}

}  // namespace UI::TrayMenu::D2DContext
