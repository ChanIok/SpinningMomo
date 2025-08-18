module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <iostream>

module UI.ContextMenu.D2DContext;

import std;
import Core.State;
import UI.AppWindow.Types;
import UI.AppWindow.State;
import UI.ContextMenu.State;
import Utils.Logger;

namespace {

using UI::ContextMenu::State::ContextMenuState;

auto create_brushes_for_target(ID2D1HwndRenderTarget* target, ID2D1SolidColorBrush** white_brush,
                               ID2D1SolidColorBrush** text_brush,
                               ID2D1SolidColorBrush** separator_brush,
                               ID2D1SolidColorBrush** hover_brush,
                               ID2D1SolidColorBrush** indicator_brush) -> bool {
  if (FAILED(target->CreateSolidColorBrush(UI::AppWindow::Colors::WHITE, white_brush)))
    return false;
  if (FAILED(target->CreateSolidColorBrush(UI::AppWindow::Colors::TEXT, text_brush))) return false;
  if (FAILED(target->CreateSolidColorBrush(UI::AppWindow::Colors::SEPARATOR, separator_brush)))
    return false;
  if (FAILED(target->CreateSolidColorBrush(UI::AppWindow::Colors::HOVER, hover_brush)))
    return false;
  if (FAILED(target->CreateSolidColorBrush(UI::AppWindow::Colors::INDICATOR, indicator_brush)))
    return false;
  return true;
}

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

namespace UI::ContextMenu::D2DContext {

auto initialize_context_menu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (menu_state.main_menu_d2d_ready) return true;

  if (!state.app_window || !state.app_window->d2d_context.is_initialized ||
      !state.app_window->d2d_context.factory) {
    return false;
  }
  const auto& d2d_context = state.app_window->d2d_context;

  RECT rc;
  GetClientRect(hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d_context.factory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size),
      &menu_state.render_target);

  if (FAILED(hr)) {
    cleanup_context_menu(state);
    return false;
  }

  if (!create_brushes_for_target(menu_state.render_target, &menu_state.white_brush,
                                 &menu_state.text_brush, &menu_state.separator_brush,
                                 &menu_state.hover_brush, &menu_state.indicator_brush)) {
    cleanup_context_menu(state);
    return false;
  }

  menu_state.main_menu_d2d_ready = true;
  return true;
}

auto cleanup_context_menu(Core::State::AppState& state) -> void {
  auto& menu_state = *state.context_menu;
  release_brushes(&menu_state.white_brush, &menu_state.text_brush, &menu_state.separator_brush,
                  &menu_state.hover_brush, &menu_state.indicator_brush);
  if (menu_state.render_target) {
    menu_state.render_target->Release();
    menu_state.render_target = nullptr;
  }
  menu_state.main_menu_d2d_ready = false;
}

auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (menu_state.submenu_d2d_ready) return true;

  if (!state.app_window || !state.app_window->d2d_context.is_initialized ||
      !state.app_window->d2d_context.factory) {
    Logger().error("AppWindow D2D not initialized or factory is null");
    return false;
  }
  const auto& d2d_context = state.app_window->d2d_context;

  RECT rc;
  GetClientRect(hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d_context.factory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size),
      &menu_state.submenu_render_target);

  if (FAILED(hr)) {
    Logger().error("Failed to create submenu render target, HRESULT: 0x{:X}", hr);
    cleanup_submenu(state);
    return false;
  }

  if (!create_brushes_for_target(
          menu_state.submenu_render_target, &menu_state.submenu_white_brush,
          &menu_state.submenu_text_brush, &menu_state.submenu_separator_brush,
          &menu_state.submenu_hover_brush, &menu_state.submenu_indicator_brush)) {
    Logger().error("Failed to create submenu brushes");
    cleanup_submenu(state);
    return false;
  }

  menu_state.submenu_d2d_ready = true;
  return true;
}

auto cleanup_submenu(Core::State::AppState& state) -> void {
  auto& menu_state = *state.context_menu;
  release_brushes(&menu_state.submenu_white_brush, &menu_state.submenu_text_brush,
                  &menu_state.submenu_separator_brush, &menu_state.submenu_hover_brush,
                  &menu_state.submenu_indicator_brush);
  if (menu_state.submenu_render_target) {
    menu_state.submenu_render_target->Release();
    menu_state.submenu_render_target = nullptr;
  }
  menu_state.submenu_d2d_ready = false;
}

auto resize_context_menu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.main_menu_d2d_ready || !menu_state.render_target) {
    return false;
  }
  return SUCCEEDED(menu_state.render_target->Resize(D2D1::SizeU(new_size.cx, new_size.cy)));
}

auto resize_submenu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.submenu_d2d_ready || !menu_state.submenu_render_target) {
    return false;
  }
  return SUCCEEDED(menu_state.submenu_render_target->Resize(D2D1::SizeU(new_size.cx, new_size.cy)));
}

}  // namespace UI::ContextMenu::D2DContext
