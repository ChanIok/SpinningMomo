module;

module UI.ContextMenu.D2DContext;

import Core.State;
import UI.FloatingWindow.State;
import UI.ContextMenu.State;
import Utils.Logger;
import Features.Settings.State;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <windows.h>;

namespace {

using UI::ContextMenu::State::RenderSurface;

auto hex_with_alpha_to_color_f(const std::string& hex_color) -> D2D1_COLOR_F {
  std::string color_str = hex_color;
  if (color_str.starts_with("#")) {
    color_str = color_str.substr(1);
  }

  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;

  if (color_str.length() == 8) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
    a = std::stoi(color_str.substr(6, 2), nullptr, 16) / 255.0f;
  } else if (color_str.length() >= 6) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
  }

  return D2D1::ColorF(r, g, b, a);
}

auto force_opaque_hex_color(std::string hex_color) -> std::string {
  if (hex_color.empty()) {
    return hex_color;
  }

  const bool has_hash = hex_color.starts_with("#");
  std::string color = has_hash ? hex_color.substr(1) : hex_color;

  if (color.length() >= 8) {
    color = color.substr(0, 6) + "FF";
  } else if (color.length() == 6) {
    color += "FF";
  }

  return has_hash ? "#" + color : color;
}

auto create_brush_from_hex(ID2D1RenderTarget* target, const std::string& hex_color,
                           ID2D1SolidColorBrush** brush) -> bool {
  return SUCCEEDED(target->CreateSolidColorBrush(hex_with_alpha_to_color_f(hex_color), brush));
}

auto has_floating_d2d_context(const Core::State::AppState& state) -> bool {
  return state.floating_window && state.floating_window->d2d_context.is_initialized &&
         state.floating_window->d2d_context.factory &&
         state.floating_window->d2d_context.write_factory;
}

auto release_brushes(RenderSurface& surface) -> void {
  if (surface.background_brush) {
    surface.background_brush->Release();
    surface.background_brush = nullptr;
  }
  if (surface.text_brush) {
    surface.text_brush->Release();
    surface.text_brush = nullptr;
  }
  if (surface.separator_brush) {
    surface.separator_brush->Release();
    surface.separator_brush = nullptr;
  }
  if (surface.hover_brush) {
    surface.hover_brush->Release();
    surface.hover_brush = nullptr;
  }
  if (surface.indicator_brush) {
    surface.indicator_brush->Release();
    surface.indicator_brush = nullptr;
  }
}

auto cleanup_surface_bitmap(RenderSurface& surface) -> void {
  if (surface.old_bitmap && surface.memory_dc) {
    SelectObject(surface.memory_dc, surface.old_bitmap);
    surface.old_bitmap = nullptr;
  }
  if (surface.dib_bitmap) {
    DeleteObject(surface.dib_bitmap);
    surface.dib_bitmap = nullptr;
  }
  surface.bitmap_bits = nullptr;
  surface.bitmap_size = {0, 0};
}

auto cleanup_surface(RenderSurface& surface) -> void {
  release_brushes(surface);

  if (surface.render_target) {
    surface.render_target->Release();
    surface.render_target = nullptr;
  }

  cleanup_surface_bitmap(surface);

  if (surface.memory_dc) {
    DeleteDC(surface.memory_dc);
    surface.memory_dc = nullptr;
  }

  surface.is_ready = false;
}

auto create_brushes_for_surface(Core::State::AppState& state, RenderSurface& surface) -> bool {
  const auto& colors = state.settings->raw.ui.floating_window_colors;
  return create_brush_from_hex(surface.render_target, force_opaque_hex_color(colors.background),
                               &surface.background_brush) &&
         create_brush_from_hex(surface.render_target, colors.text, &surface.text_brush) &&
         create_brush_from_hex(surface.render_target, force_opaque_hex_color(colors.separator),
                               &surface.separator_brush) &&
         create_brush_from_hex(surface.render_target, force_opaque_hex_color(colors.hover),
                               &surface.hover_brush) &&
         create_brush_from_hex(surface.render_target, colors.indicator, &surface.indicator_brush);
}

auto ensure_memory_dc(RenderSurface& surface) -> bool {
  if (surface.memory_dc) {
    return true;
  }

  HDC screen_dc = GetDC(nullptr);
  surface.memory_dc = CreateCompatibleDC(screen_dc);
  ReleaseDC(nullptr, screen_dc);
  return surface.memory_dc != nullptr;
}

auto create_bitmap_for_surface(RenderSurface& surface, const SIZE& size) -> bool {
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  if (!ensure_memory_dc(surface)) {
    return false;
  }

  cleanup_surface_bitmap(surface);

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size.cx;
  bmi.bmiHeader.biHeight = -size.cy;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  surface.dib_bitmap =
      CreateDIBSection(surface.memory_dc, &bmi, DIB_RGB_COLORS, &surface.bitmap_bits, nullptr, 0);
  if (!surface.dib_bitmap) {
    return false;
  }

  surface.old_bitmap = SelectObject(surface.memory_dc, surface.dib_bitmap);
  surface.bitmap_size = size;
  return true;
}

auto create_render_target(ID2D1Factory7* factory, RenderSurface& surface) -> bool {
  if (surface.render_target) {
    return true;
  }

  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0,
      D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);

  return SUCCEEDED(factory->CreateDCRenderTarget(&props, &surface.render_target));
}

auto bind_surface(RenderSurface& surface) -> bool {
  if (!surface.render_target || !surface.memory_dc || surface.bitmap_size.cx <= 0 ||
      surface.bitmap_size.cy <= 0) {
    return false;
  }

  RECT binding_rect = {0, 0, surface.bitmap_size.cx, surface.bitmap_size.cy};
  return SUCCEEDED(surface.render_target->BindDC(surface.memory_dc, &binding_rect));
}

auto initialize_surface(Core::State::AppState& state, RenderSurface& surface, const SIZE& size)
    -> bool {
  if (!has_floating_d2d_context(state)) {
    return false;
  }

  if (surface.is_ready && surface.bitmap_size.cx == size.cx && surface.bitmap_size.cy == size.cy) {
    return true;
  }

  cleanup_surface(surface);

  if (!create_render_target(state.floating_window->d2d_context.factory, surface)) {
    cleanup_surface(surface);
    return false;
  }

  if (!create_bitmap_for_surface(surface, size)) {
    cleanup_surface(surface);
    return false;
  }

  if (!bind_surface(surface)) {
    cleanup_surface(surface);
    return false;
  }

  if (!create_brushes_for_surface(state, surface)) {
    cleanup_surface(surface);
    return false;
  }

  surface.is_ready = true;
  return true;
}

auto resize_surface(RenderSurface& surface, const SIZE& new_size) -> bool {
  if (!surface.is_ready || new_size.cx <= 0 || new_size.cy <= 0) {
    return false;
  }

  if (surface.bitmap_size.cx == new_size.cx && surface.bitmap_size.cy == new_size.cy) {
    return true;
  }

  if (!create_bitmap_for_surface(surface, new_size)) {
    return false;
  }

  return bind_surface(surface);
}

auto get_client_size(HWND hwnd) -> SIZE {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  return {rc.right - rc.left, rc.bottom - rc.top};
}

}  // namespace

namespace UI::ContextMenu::D2DContext {

auto initialize_text_format(Core::State::AppState& state) -> bool {
  auto& menu_state = *state.context_menu;

  if (!has_floating_d2d_context(state)) {
    return false;
  }

  if (menu_state.text_format) {
    menu_state.text_format->Release();
    menu_state.text_format = nullptr;
  }

  HRESULT hr = state.floating_window->d2d_context.write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(menu_state.layout.font_size), L"zh-CN",
      &menu_state.text_format);
  if (FAILED(hr) || !menu_state.text_format) {
    return false;
  }

  menu_state.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  menu_state.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  return true;
}

auto initialize_context_menu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.text_format && !initialize_text_format(state)) {
    return false;
  }

  return initialize_surface(state, menu_state.main_surface, get_client_size(hwnd));
}

auto cleanup_context_menu(Core::State::AppState& state) -> void {
  cleanup_surface(state.context_menu->main_surface);

  auto& menu_state = *state.context_menu;
  if (menu_state.text_format) {
    menu_state.text_format->Release();
    menu_state.text_format = nullptr;
  }
}

auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.text_format && !initialize_text_format(state)) {
    return false;
  }

  return initialize_surface(state, menu_state.submenu_surface, get_client_size(hwnd));
}

auto cleanup_submenu(Core::State::AppState& state) -> void {
  cleanup_surface(state.context_menu->submenu_surface);
}

auto resize_context_menu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  return resize_surface(state.context_menu->main_surface, new_size);
}

auto resize_submenu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  return resize_surface(state.context_menu->submenu_surface, new_size);
}

}  // namespace UI::ContextMenu::D2DContext
