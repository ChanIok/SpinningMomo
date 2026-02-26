module;

module UI.ContextMenu.D2DContext;

import Core.State;
import UI.FloatingWindow.Types;
import UI.FloatingWindow.State;
import UI.ContextMenu.State;
import Utils.Logger;
import Features.Settings.State;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <windows.h>;

namespace {

// 辅助函数：从包含透明度的十六进制颜色字符串创建 D2D1_COLOR_F
auto hex_with_alpha_to_color_f(const std::string& hex_color) -> D2D1_COLOR_F {
  // 忽略透明度，只使用 RRGGBB 部分
  std::string color_str = hex_color;
  if (color_str.starts_with("#")) {
    color_str = color_str.substr(1);
  }

  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

  if (color_str.length() >= 6) {
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

auto create_brushes_for_target(Core::State::AppState& state, ID2D1HwndRenderTarget* target,
                               ID2D1SolidColorBrush** background_brush,
                               ID2D1SolidColorBrush** text_brush,
                               ID2D1SolidColorBrush** separator_brush,
                               ID2D1SolidColorBrush** hover_brush,
                               ID2D1SolidColorBrush** indicator_brush) -> bool {
  const auto& colors = state.settings->raw.ui.floating_window_colors;

  if (!create_brush_from_hex(target, colors.background, background_brush)) return false;
  if (!create_brush_from_hex(target, colors.text, text_brush)) return false;
  if (!create_brush_from_hex(target, colors.separator, separator_brush)) return false;
  if (!create_brush_from_hex(target, colors.hover, hover_brush)) return false;
  if (!create_brush_from_hex(target, colors.indicator, indicator_brush)) return false;
  return true;
}

auto has_floating_d2d_context(const Core::State::AppState& state) -> bool {
  return state.floating_window && state.floating_window->d2d_context.is_initialized &&
         state.floating_window->d2d_context.factory;
}

auto create_hwnd_render_target(ID2D1Factory7* factory, HWND hwnd,
                               ID2D1HwndRenderTarget** render_target) -> HRESULT {
  RECT rc;
  GetClientRect(hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

  // 强制 96 DPI，避免 HwndRenderTarget 自动按系统 DPI 缩放
  return factory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96.0f,
                                   96.0f),
      D2D1::HwndRenderTargetProperties(hwnd, size), render_target);
}

auto release_brushes(ID2D1SolidColorBrush** background_brush, ID2D1SolidColorBrush** text_brush,
                     ID2D1SolidColorBrush** separator_brush, ID2D1SolidColorBrush** hover_brush,
                     ID2D1SolidColorBrush** indicator_brush) -> void {
  if (*background_brush) {
    (*background_brush)->Release();
    *background_brush = nullptr;
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

  if (!has_floating_d2d_context(state)) {
    return false;
  }
  const auto& d2d_context = state.floating_window->d2d_context;

  HRESULT hr = create_hwnd_render_target(d2d_context.factory, hwnd, &menu_state.render_target);

  if (FAILED(hr)) {
    cleanup_context_menu(state);
    return false;
  }

  if (!create_brushes_for_target(state, menu_state.render_target, &menu_state.background_brush,
                                 &menu_state.text_brush, &menu_state.separator_brush,
                                 &menu_state.hover_brush, &menu_state.indicator_brush)) {
    cleanup_context_menu(state);
    return false;
  }

  // 创建独立的文本格式（使用 DPI 缩放后的字号）
  if (menu_state.text_format) {
    menu_state.text_format->Release();
    menu_state.text_format = nullptr;
  }
  hr = d2d_context.write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(menu_state.layout.font_size), L"zh-CN",
      &menu_state.text_format);
  if (FAILED(hr) || !menu_state.text_format) {
    cleanup_context_menu(state);
    return false;
  }
  menu_state.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  menu_state.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  menu_state.main_menu_d2d_ready = true;
  return true;
}

auto cleanup_context_menu(Core::State::AppState& state) -> void {
  auto& menu_state = *state.context_menu;
  release_brushes(&menu_state.background_brush, &menu_state.text_brush, &menu_state.separator_brush,
                  &menu_state.hover_brush, &menu_state.indicator_brush);
  if (menu_state.text_format) {
    menu_state.text_format->Release();
    menu_state.text_format = nullptr;
  }
  if (menu_state.render_target) {
    menu_state.render_target->Release();
    menu_state.render_target = nullptr;
  }
  menu_state.main_menu_d2d_ready = false;
}

auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (menu_state.submenu_d2d_ready) return true;

  if (!has_floating_d2d_context(state)) {
    Logger().error("FloatingWindow D2D not initialized or factory is null");
    return false;
  }
  const auto& d2d_context = state.floating_window->d2d_context;

  HRESULT hr =
      create_hwnd_render_target(d2d_context.factory, hwnd, &menu_state.submenu_render_target);

  if (FAILED(hr)) {
    Logger().error("Failed to create submenu render target, HRESULT: 0x{:X}", hr);
    cleanup_submenu(state);
    return false;
  }

  if (!create_brushes_for_target(
          state, menu_state.submenu_render_target, &menu_state.submenu_background_brush,
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
  release_brushes(&menu_state.submenu_background_brush, &menu_state.submenu_text_brush,
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
