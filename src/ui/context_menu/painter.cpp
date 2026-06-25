module;

module UI.ContextMenu.Painter;

import std;
import Core.State;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Interaction;
import UI.ContextMenu.RenderContext;
import Utils.Logger;
import <d2d1_3.h>;
import <windows.h>;

namespace UI::ContextMenu::Painter {

struct AnimationAlphaState {
  State::RenderResources* resources = nullptr;
  float original_alpha = 1.0f;
  bool applied = false;
};

// 临时将所有 brush 的 alpha 乘以动画 opacity，返回可恢复的快照
auto apply_animation_alpha(State::RenderResources& resources,
                           const Types::MenuOpenAnimation& animation) -> AnimationAlphaState {
  if (!resources.device_context) {
    return {};
  }

  const bool needs_animation = animation.active || animation.opacity < 0.999f;
  if (!needs_animation) {
    return {};
  }

  const float opacity = std::clamp(animation.opacity, 0.0f, 1.0f);
  float original_alpha = 1.0f;

  ID2D1SolidColorBrush* brushes[] = {
      resources.background_brush.get(), resources.text_brush.get(),
      resources.separator_brush.get(),  resources.hover_brush.get(),
      resources.indicator_brush.get(),
  };

  for (auto* brush : brushes) {
    if (!brush) {
      continue;
    }
    auto color = brush->GetColor();
    if (original_alpha == 1.0f) {
      original_alpha = color.a;
    }
    color.a *= opacity;
    brush->SetColor(color);
  }

  return {&resources, original_alpha, true};
}

// 将 brush alpha 恢复到动画前的值，使后续正常绘制不受影响
auto restore_animation_alpha(const AnimationAlphaState& state) -> void {
  if (!state.applied || !state.resources) {
    return;
  }

  ID2D1SolidColorBrush* brushes[] = {
      state.resources->background_brush.get(), state.resources->text_brush.get(),
      state.resources->separator_brush.get(),  state.resources->hover_brush.get(),
      state.resources->indicator_brush.get(),
  };

  for (auto* brush : brushes) {
    if (!brush) {
      continue;
    }
    auto color = brush->GetColor();
    color.a = state.original_alpha;
    brush->SetColor(color);
  }
}

auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

auto present_surface(State::RenderResources& render_resources, const char* label) -> void {
  if (!render_resources.swap_chain) {
    return;
  }

  const HRESULT hr = render_resources.swap_chain->Present(0, 0);
  if (FAILED(hr)) {
    Logger().error("{} present error: 0x{:X}", label, hr);
  }
}

// 绘制主菜单一帧：透明清屏 → 叠动画 alpha → 背景+菜单项 → 恢复 alpha → 提交
auto paint_context_menu(Core::State::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& render_resources = menu_state.main_render_resources;
  if (!render_resources.is_ready || !render_resources.device_context || !menu_state.text_format) {
    return;
  }

  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  {
    const auto rect_f = rect_to_d2d(client_rect);
    auto alpha_state = apply_animation_alpha(render_resources, menu_state.main_animation);
    draw_menu_background(state, rect_f);
    draw_menu_items(state, rect_f);
    restore_animation_alpha(alpha_state);
  }

  const HRESULT hr = render_resources.device_context->EndDraw();
  // GPU 设备丢失时重建 D2D 资源，下次 paint 会正常
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Main menu render target needs recreation");
    UI::ContextMenu::RenderContext::initialize_context_menu(state, menu_state.hwnd);
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Main menu paint error: 0x{:X}", hr);
    return;
  }

  present_surface(render_resources, "Main menu");
}

auto draw_menu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render_resources = state.context_menu->main_render_resources;
  render_resources.device_context->FillRectangle(rect, render_resources.background_brush.get());
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

auto draw_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& render_resources = state.context_menu->main_render_resources;
  render_resources.device_context->FillRectangle(separator_rect,
                                                 render_resources.separator_brush.get());
}

// 绘制子菜单一帧，流程与 paint_context_menu 对称
auto paint_submenu(Core::State::AppState& state, const RECT& client_rect) -> void {
  auto& menu_state = *state.context_menu;
  auto& render_resources = menu_state.submenu_render_resources;
  if (!render_resources.is_ready || !render_resources.device_context || !menu_state.text_format) {
    return;
  }

  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  {
    const auto rect_f = rect_to_d2d(client_rect);
    auto alpha_state = apply_animation_alpha(render_resources, menu_state.submenu_animation);
    draw_submenu_background(state, rect_f);
    draw_submenu_items(state, rect_f);
    restore_animation_alpha(alpha_state);
  }

  const HRESULT hr = render_resources.device_context->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    Logger().warn("Submenu render target needs recreation");
    UI::ContextMenu::RenderContext::initialize_submenu(state, menu_state.submenu_hwnd);
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Submenu paint error: 0x{:X}", hr);
    return;
  }

  present_surface(render_resources, "Submenu");
}

auto draw_submenu_background(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render_resources = state.context_menu->submenu_render_resources;
  render_resources.device_context->FillRectangle(rect, render_resources.background_brush.get());
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

auto draw_submenu_separator(Core::State::AppState& state, const D2D1_RECT_F& separator_rect)
    -> void {
  const auto& render_resources = state.context_menu->submenu_render_resources;
  render_resources.device_context->FillRectangle(separator_rect,
                                                 render_resources.separator_brush.get());
}

}  // namespace UI::ContextMenu::Painter
