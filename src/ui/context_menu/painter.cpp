module;

module UI.ContextMenu.Painter;

import std;
import Core.State;
import UI.FloatingWindow.Types;
import UI.FloatingWindow.State;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Interaction;
import UI.ContextMenu.D2DContext;
import Utils.Logger;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <windows.h>;

namespace {

using UI::ContextMenu::State::ContextMenuState;
using UI::ContextMenu::State::RenderSurface;

auto present_surface(HWND hwnd, const RenderSurface& surface, const POINT& position) -> void {
  if (!hwnd || !surface.memory_dc || surface.bitmap_size.cx <= 0 || surface.bitmap_size.cy <= 0) {
    return;
  }

  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;

  POINT src_point = {0, 0};
  POINT dst_point = position;
  SIZE size = surface.bitmap_size;

  UpdateLayeredWindow(hwnd, nullptr, &dst_point, &size, surface.memory_dc, &src_point, 0, &blend,
                      ULW_ALPHA);
}

auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

}  // namespace

namespace UI::ContextMenu::Painter {

auto paint_context_menu(Core::State::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& surface = menu_state.main_surface;
  if (!surface.is_ready || !surface.render_target || !menu_state.text_format) {
    return;
  }

  surface.render_target->BeginDraw();
  surface.render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  const auto rect_f = rect_to_d2d(client_rect);
  draw_menu_background(state, rect_f);
  draw_menu_items(state, rect_f);

  HRESULT hr = surface.render_target->EndDraw();
  if (FAILED(hr)) {
    if (hr == D2DERR_RECREATE_TARGET) {
      Logger().warn("Main menu render target needs recreation");
    } else {
      Logger().error("Main menu paint error: 0x{:X}", hr);
    }
    return;
  }

  present_surface(menu_state.hwnd, surface, menu_state.position);
}

auto draw_menu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& surface = state.context_menu->main_surface;
  surface.render_target->FillRectangle(rect, surface.background_brush);
}

auto draw_menu_items(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const int highlight_index = UI::ContextMenu::Interaction::get_main_highlight_index(state);
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < menu_state.items.size(); ++i) {
    const auto& item = menu_state.items[i];
    const bool is_hovered = static_cast<int>(i) == highlight_index;
    if (item.type == Types::MenuItemType::Separator) {
      const float separator_height = static_cast<float>(layout.separator_height);
      const D2D1_RECT_F separator_rect = D2D1::RectF(
          rect.left + static_cast<float>(layout.text_padding), current_y,
          rect.right - static_cast<float>(layout.text_padding), current_y + separator_height);
      draw_separator(state, separator_rect);
      current_y += separator_height;
    } else {
      const float item_height = static_cast<float>(layout.item_height);
      const D2D1_RECT_F item_rect =
          D2D1::RectF(rect.left, current_y, rect.right, current_y + item_height);
      draw_single_menu_item(state, item, item_rect, is_hovered);
      current_y += item_height;
    }
  }
}

auto draw_single_menu_item(Core::State::AppState& state, const Types::MenuItem& item,
                           const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& surface = menu_state.main_surface;
  const auto& layout = menu_state.layout;

  if (is_hovered && item.is_enabled) {
    surface.render_target->FillRectangle(item_rect, surface.hover_brush);
  }

  const D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush = item.is_enabled ? surface.text_brush : surface.separator_brush;
  surface.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                  menu_state.text_format, text_rect, text_brush);

  if (item.has_submenu()) {
    const float arrow_height = static_cast<float>(layout.font_size) * 0.6f;
    const float arrow_width = arrow_height * 0.6f;
    const float arrow_x = item_rect.right - static_cast<float>(layout.text_padding) - arrow_width;
    const float arrow_y = item_rect.top + (item_rect.bottom - item_rect.top - arrow_height) / 2;

    D2D1_POINT_2F points[3] = {
        D2D1::Point2F(arrow_x, arrow_y),
        D2D1::Point2F(arrow_x + arrow_width, arrow_y + arrow_height / 2),
        D2D1::Point2F(arrow_x, arrow_y + arrow_height),
    };

    const float stroke_width = static_cast<float>(layout.font_size) * 0.1f;
    surface.render_target->DrawLine(points[0], points[1], text_brush, stroke_width);
    surface.render_target->DrawLine(points[1], points[2], text_brush, stroke_width);
  } else if (item.is_checked) {
    const float check_size = static_cast<float>(layout.font_size) * 0.6f;
    const float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    const float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    const D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    surface.render_target->FillRectangle(check_rect, surface.indicator_brush);
  }
}

auto draw_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& surface = state.context_menu->main_surface;
  surface.render_target->FillRectangle(separator_rect, surface.separator_brush);
}

auto paint_submenu(Core::State::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& surface = menu_state.submenu_surface;
  if (!surface.is_ready || !surface.render_target || !menu_state.text_format) {
    return;
  }

  surface.render_target->BeginDraw();
  surface.render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  const auto rect_f = rect_to_d2d(client_rect);
  draw_submenu_background(state, rect_f);
  draw_submenu_items(state, rect_f);

  HRESULT hr = surface.render_target->EndDraw();
  if (FAILED(hr)) {
    if (hr == D2DERR_RECREATE_TARGET) {
      Logger().warn("Submenu render target needs recreation");
    } else {
      Logger().error("Submenu paint error: 0x{:X}", hr);
    }
    return;
  }

  present_surface(menu_state.submenu_hwnd, surface, menu_state.submenu_position);
}

auto draw_submenu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& surface = state.context_menu->submenu_surface;
  surface.render_target->FillRectangle(rect, surface.background_brush);
}

auto draw_submenu_items(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < current_submenu.size(); ++i) {
    const auto& item = current_submenu[i];
    const bool is_hovered = static_cast<int>(i) == menu_state.interaction.submenu_hover_index;
    if (item.type == Types::MenuItemType::Separator) {
      const float separator_height = static_cast<float>(layout.separator_height);
      const D2D1_RECT_F separator_rect = D2D1::RectF(
          rect.left + static_cast<float>(layout.text_padding), current_y,
          rect.right - static_cast<float>(layout.text_padding), current_y + separator_height);
      draw_submenu_separator(state, separator_rect);
      current_y += separator_height;
    } else {
      const float item_height = static_cast<float>(layout.item_height);
      const D2D1_RECT_F item_rect =
          D2D1::RectF(rect.left, current_y, rect.right, current_y + item_height);
      draw_submenu_single_item(state, item, item_rect, is_hovered);
      current_y += item_height;
    }
  }
}

auto draw_submenu_single_item(Core::State::AppState& state, const Types::MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& surface = menu_state.submenu_surface;
  const auto& layout = menu_state.layout;

  if (is_hovered && item.is_enabled) {
    surface.render_target->FillRectangle(item_rect, surface.hover_brush);
  }

  const D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush = item.is_enabled ? surface.text_brush : surface.separator_brush;
  surface.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                  menu_state.text_format, text_rect, text_brush);

  if (item.is_checked) {
    const float check_size = static_cast<float>(layout.font_size) * 0.8f;
    const float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    const float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    const D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    surface.render_target->FillRectangle(check_rect, surface.indicator_brush);
  }
}

auto draw_submenu_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect)
    -> void {
  const auto& surface = state.context_menu->submenu_surface;
  surface.render_target->FillRectangle(separator_rect, surface.separator_brush);
}

}  // namespace UI::ContextMenu::Painter
