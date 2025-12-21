module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <string>

module UI.ContextMenu.Painter;

import std;
import Core.State;
import UI.FloatingWindow.Types;
import UI.FloatingWindow.State;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Layout;
import Utils.Logger;

namespace UI::ContextMenu::Painter {

using State::ContextMenuState;

auto paint_context_menu(Core::State::AppState& state, const RECT& client_rect) -> void {
  const auto& menu_state = *state.context_menu;
  if (!menu_state.main_menu_d2d_ready || !menu_state.render_target) {
    return;
  }
  const auto& d2d = state.floating_window->d2d_context;
  if (!d2d.is_initialized) {
    return;
  }

  menu_state.render_target->BeginDraw();
  const auto rect_f = UI::FloatingWindow::rect_to_d2d(client_rect);
  draw_menu_background(state, rect_f);
  draw_menu_items(state, rect_f);
  HRESULT hr = menu_state.render_target->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Main menu render target needs recreation");
    const_cast<ContextMenuState&>(menu_state).main_menu_d2d_ready = false;
  } else if (FAILED(hr)) {
    Logger().error("Main menu paint error: 0x{:X}", hr);
  }
}

auto draw_menu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  menu_state.render_target->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.95f));
  const auto& layout = menu_state.layout;
  D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(rect, static_cast<float>(layout.border_radius),
                                                     static_cast<float>(layout.border_radius));
  menu_state.render_target->FillRoundedRectangle(rounded_rect, menu_state.background_brush);
}

auto draw_menu_items(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < menu_state.items.size(); ++i) {
    const auto& item = menu_state.items[i];
    bool is_hovered = (static_cast<int>(i) == menu_state.interaction.hover_index);
    if (item.type == Types::MenuItemType::Separator) {
      float separator_height = static_cast<float>(layout.separator_height);
      D2D1_RECT_F separator_rect = D2D1::RectF(
          rect.left + static_cast<float>(layout.text_padding), current_y,
          rect.right - static_cast<float>(layout.text_padding), current_y + separator_height);
      draw_separator(state, separator_rect);
      current_y += separator_height;
    } else {
      float item_height = static_cast<float>(layout.item_height);
      D2D1_RECT_F item_rect =
          D2D1::RectF(rect.left, current_y, rect.right, current_y + item_height);
      draw_single_menu_item(state, item, item_rect, is_hovered);
      current_y += item_height;
    }
  }
}

auto draw_single_menu_item(Core::State::AppState& state, const Types::MenuItem& item,
                           const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& d2d = state.floating_window->d2d_context;
  const auto& layout = menu_state.layout;
  if (is_hovered && item.is_enabled) {
    menu_state.render_target->FillRectangle(item_rect, menu_state.hover_brush);
  }
  D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush =
      item.is_enabled ? menu_state.text_brush : menu_state.separator_brush;
  menu_state.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                     d2d.text_format, text_rect, text_brush);

  if (item.has_submenu()) {
    float arrow_height = static_cast<float>(layout.font_size) * 0.6f;
    float arrow_width = arrow_height * 0.8f;

    float arrow_x = item_rect.right - static_cast<float>(layout.text_padding) - arrow_width;
    float arrow_y = item_rect.top + (item_rect.bottom - item_rect.top - arrow_height) / 2;

    D2D1_POINT_2F points[3] = {
        D2D1::Point2F(arrow_x, arrow_y),                                   // 左上点
        D2D1::Point2F(arrow_x + arrow_width, arrow_y + arrow_height / 2),  // 右中点
        D2D1::Point2F(arrow_x, arrow_y + arrow_height)                     // 左下点
    };

    menu_state.render_target->DrawLine(points[0], points[1], text_brush, 1.2f);
    menu_state.render_target->DrawLine(points[1], points[2], text_brush, 1.2f);
  } else if (item.is_checked) {
    float check_size = static_cast<float>(layout.font_size) * 0.6f;
    float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    menu_state.render_target->FillRectangle(check_rect, menu_state.indicator_brush);
  }
}

auto draw_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& menu_state = *state.context_menu;
  menu_state.render_target->FillRectangle(separator_rect, menu_state.separator_brush);
}

auto paint_submenu(Core::State::AppState& state, const RECT& client_rect) -> void {
  const auto& menu_state = *state.context_menu;
  if (!menu_state.submenu_d2d_ready || !menu_state.submenu_render_target) {
    return;
  }
  const auto& d2d = state.floating_window->d2d_context;
  if (!d2d.is_initialized) {
    return;
  }
  menu_state.submenu_render_target->BeginDraw();
  const auto rect_f = UI::FloatingWindow::rect_to_d2d(client_rect);
  draw_submenu_background(state, rect_f);
  draw_submenu_items(state, rect_f);
  HRESULT hr = menu_state.submenu_render_target->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Submenu render target needs recreation");
    const_cast<ContextMenuState&>(menu_state).submenu_d2d_ready = false;
  } else if (FAILED(hr)) {
    Logger().error("Submenu paint error: 0x{:X}", hr);
  }
}

auto draw_submenu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  menu_state.submenu_render_target->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.95f));
  D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(rect, static_cast<float>(layout.border_radius),
                                                     static_cast<float>(layout.border_radius));
  menu_state.submenu_render_target->FillRoundedRectangle(rounded_rect,
                                                         menu_state.submenu_background_brush);
}

auto draw_submenu_items(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < current_submenu.size(); ++i) {
    const auto& item = current_submenu[i];
    bool is_hovered = (static_cast<int>(i) == menu_state.interaction.submenu_hover_index);
    if (item.type == Types::MenuItemType::Separator) {
      float separator_height = static_cast<float>(layout.separator_height);
      D2D1_RECT_F separator_rect = D2D1::RectF(
          rect.left + static_cast<float>(layout.text_padding), current_y,
          rect.right - static_cast<float>(layout.text_padding), current_y + separator_height);
      draw_submenu_separator(state, separator_rect);
      current_y += separator_height;
    } else {
      float item_height = static_cast<float>(layout.item_height);
      D2D1_RECT_F item_rect =
          D2D1::RectF(rect.left, current_y, rect.right, current_y + item_height);
      draw_submenu_single_item(state, item, item_rect, is_hovered);
      current_y += item_height;
    }
  }
}

auto draw_submenu_single_item(Core::State::AppState& state, const Types::MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& d2d = state.floating_window->d2d_context;
  const auto& layout = menu_state.layout;
  if (is_hovered && item.is_enabled) {
    menu_state.submenu_render_target->FillRectangle(item_rect, menu_state.submenu_hover_brush);
  }
  D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush =
      item.is_enabled ? menu_state.submenu_text_brush : menu_state.submenu_separator_brush;
  menu_state.submenu_render_target->DrawText(item.text.c_str(),
                                             static_cast<UINT32>(item.text.length()),
                                             d2d.text_format, text_rect, text_brush);
  if (item.is_checked) {
    float check_size = static_cast<float>(layout.font_size) * 0.8f;
    float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    menu_state.submenu_render_target->FillRectangle(check_rect, menu_state.submenu_indicator_brush);
  }
}

auto draw_submenu_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect)
    -> void {
  const auto& menu_state = *state.context_menu;
  menu_state.submenu_render_target->FillRectangle(separator_rect,
                                                  menu_state.submenu_separator_brush);
}

}  // namespace UI::ContextMenu::Painter
