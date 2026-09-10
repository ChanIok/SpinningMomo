#include "ui/context_menu/painter.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"

#include "core/state/app_state.hpp"
#include "ui/context_menu/interaction.hpp"
#include "ui/context_menu/render_context.hpp"
#include "ui/context_menu/state.hpp"
#include "ui/context_menu/types.hpp"
#include "utils/logger/logger.hpp"

namespace ui::context_menu::painter {

auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

auto present_surface(RenderResources& render_resources, const char* label) -> void {
  if (!render_resources.swap_chain) {
    return;
  }

  const HRESULT hr = render_resources.swap_chain->Present(0, 0);
  if (FAILED(hr)) {
    Logger().error("{} present error: 0x{:X}", label, hr);
  }
}

// 绘制主菜单：清空背景 → 绘制菜单背景与菜单项 → 结束绘制并提交交换链（整体淡入由合成器处理）
auto paint_context_menu(core::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& render_resources = menu_state.main_render_resources;
  if (!render_resources.is_ready || !render_resources.device_context || !menu_state.text_format) {
    return;
  }

  // 开始 D2D 绘制并清空为透明背景
  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  // 绘制菜单背景与所有菜单项内容
  {
    const auto rect_f = rect_to_d2d(client_rect);
    draw_menu_background(state, rect_f);
    draw_menu_items(state, rect_f);
  }

  // 结束 D2D 绘制
  const HRESULT hr = render_resources.device_context->EndDraw();
  // GPU 设备丢失时重建 D2D 资源，下次 paint 会正常
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Main menu render target needs recreation");
    ui::context_menu::render_context::initialize_context_menu(state, menu_state.hwnd);
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Main menu paint error: 0x{:X}", hr);
    return;
  }

  present_surface(render_resources, "Main menu");
}

auto draw_menu_background(core::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render_resources = state.context_menu->main_render_resources;
  render_resources.device_context->FillRectangle(rect, render_resources.background_brush.get());
}

auto draw_menu_items(core::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const int highlight_index = ui::context_menu::interaction::get_main_highlight_index(state);
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < menu_state.items.size(); ++i) {
    const auto& item = menu_state.items[i];
    const bool is_hovered = static_cast<int>(i) == highlight_index;
    if (item.type == MenuItemType::Separator) {
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

auto draw_single_menu_item(core::AppState& state, const MenuItem& item,
                           const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& render_resources = menu_state.main_render_resources;
  const auto& layout = menu_state.layout;

  if (is_hovered && item.is_enabled) {
    render_resources.device_context->FillRectangle(item_rect, render_resources.hover_brush.get());
  }

  const D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush =
      item.is_enabled ? render_resources.text_brush.get() : render_resources.separator_brush.get();
  render_resources.device_context->DrawText(item.text.c_str(),
                                            static_cast<UINT32>(item.text.length()),
                                            menu_state.text_format.get(), text_rect, text_brush);

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
    render_resources.device_context->DrawLine(points[0], points[1], text_brush, stroke_width);
    render_resources.device_context->DrawLine(points[1], points[2], text_brush, stroke_width);
  } else if (item.is_checked) {
    const float check_size = static_cast<float>(layout.font_size) * 0.6f;
    const float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    const float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    const D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    render_resources.device_context->FillRectangle(check_rect,
                                                   render_resources.indicator_brush.get());
  }
}

auto draw_separator(core::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& render_resources = state.context_menu->main_render_resources;
  render_resources.device_context->FillRectangle(separator_rect,
                                                 render_resources.separator_brush.get());
}

// 绘制子菜单：清空背景 → 绘制子菜单背景与项 → 结束绘制并提交交换链（整体淡入由合成器处理）
auto paint_submenu(core::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& render_resources = menu_state.submenu_render_resources;
  if (!render_resources.is_ready || !render_resources.device_context || !menu_state.text_format) {
    return;
  }

  // 开始 D2D 绘制并清空为透明背景
  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  // 绘制子菜单背景与项内容
  {
    const auto rect_f = rect_to_d2d(client_rect);
    draw_submenu_background(state, rect_f);
    draw_submenu_items(state, rect_f);
  }

  // 结束 D2D 绘制
  const HRESULT hr = render_resources.device_context->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Submenu render target needs recreation");
    ui::context_menu::render_context::initialize_submenu(state, menu_state.submenu_hwnd);
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Submenu paint error: 0x{:X}", hr);
    return;
  }

  present_surface(render_resources, "Submenu");
}

auto draw_submenu_background(core::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render_resources = state.context_menu->submenu_render_resources;
  render_resources.device_context->FillRectangle(rect, render_resources.background_brush.get());
}

auto draw_submenu_items(core::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();
  float current_y = rect.top + static_cast<float>(layout.padding);
  for (size_t i = 0; i < current_submenu.size(); ++i) {
    const auto& item = current_submenu[i];
    const bool is_hovered = static_cast<int>(i) == menu_state.interaction.submenu_hover_index;
    if (item.type == MenuItemType::Separator) {
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

auto draw_submenu_single_item(core::AppState& state, const MenuItem& item,
                              const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& menu_state = *state.context_menu;
  const auto& render_resources = menu_state.submenu_render_resources;
  const auto& layout = menu_state.layout;

  if (is_hovered && item.is_enabled) {
    render_resources.device_context->FillRectangle(item_rect, render_resources.hover_brush.get());
  }

  const D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);
  ID2D1SolidColorBrush* text_brush =
      item.is_enabled ? render_resources.text_brush.get() : render_resources.separator_brush.get();
  render_resources.device_context->DrawText(item.text.c_str(),
                                            static_cast<UINT32>(item.text.length()),
                                            menu_state.text_format.get(), text_rect, text_brush);

  if (item.is_checked) {
    const float check_size = static_cast<float>(layout.font_size) * 0.8f;
    const float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    const float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;
    const D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);
    render_resources.device_context->FillRectangle(check_rect,
                                                   render_resources.indicator_brush.get());
  }
}

auto draw_submenu_separator(core::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& render_resources = state.context_menu->submenu_render_resources;
  render_resources.device_context->FillRectangle(separator_rect,
                                                 render_resources.separator_brush.get());
}

}  // namespace ui::context_menu::painter
