module;

module UI.NotificationWindow.Painter;

import std;
import Core.State;
import Core.Notifications.Types;
import UI.NotificationWindow.RenderContext;
import UI.NotificationWindow.State;
import UI.NotificationWindow.Types;
import UI.SharedRenderResources.State;
import UI.SharedTheme;
import UI.FloatingWindow.State;
import Utils.Logger;
import Utils.System;
import <d2d1_3.h>;
import <windows.h>;

namespace UI::NotificationWindow::Painter {

struct NotificationVisualStyle {
  bool use_system_chrome = false;
  float corner_radius = 0.0f;
  float border_width = 0.0f;
  int shadow_margin = 0;
  D2D1_COLOR_F border_color{};
};

auto scale_for_dpi(int value, int dpi) -> int { return MulDiv(value, dpi, 96); }

auto stroke_width_for_dpi(int dpi) -> float { return static_cast<float>(scale_for_dpi(1, dpi)); }

auto is_exiting(NotificationWindow::NotificationAnimState state) -> bool {
  return state == NotificationWindow::NotificationAnimState::FadingOut ||
         state == NotificationWindow::NotificationAnimState::Done;
}

auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

auto color_with_opacity(D2D1_COLOR_F color, float opacity) -> D2D1_COLOR_F {
  color.a *= std::clamp(opacity, 0.0f, 1.0f);
  return color;
}

auto is_windows_11_or_newer() -> bool {
  static const bool result = []() -> bool {
    const auto version = Utils::System::get_windows_version();
    if (!version) {
      return false;
    }
    return version->major_version >= 10 && version->build_number >= 22000;
  }();
  return result;
}

auto resolve_visual_style(int dpi) -> NotificationVisualStyle {
  if (!is_windows_11_or_newer()) {
    return {};
  }

  const float border_gray = static_cast<float>(NotificationWindow::BASE_BORDER_GRAY) / 255.0f;
  return NotificationVisualStyle{
      .use_system_chrome = true,
      .corner_radius =
          static_cast<float>(scale_for_dpi(NotificationWindow::BASE_CORNER_RADIUS, dpi)),
      .border_width = stroke_width_for_dpi(dpi),
      .shadow_margin = scale_for_dpi(10, dpi),
      .border_color = D2D1::ColorF(border_gray, border_gray, border_gray,
                                   NotificationWindow::BASE_BORDER_ALPHA),
  };
}

// 通知只复用浮窗的基础配色，不复用布局和边框策略，避免把两种窗口结构绑死
auto resolve_notification_theme_colors(const Core::State::AppState& state)
    -> NotificationWindow::NotificationThemeColors {
  const auto colors = UI::SharedTheme::resolve_floating_window_theme_colors(state);
  return NotificationWindow::NotificationThemeColors{
      .background = colors.background,
      .text = colors.text,
      .hover = colors.hover,
  };
}

auto get_current_dpi(const Core::State::AppState& state) -> int {
  if (state.notification_window->host_hwnd) {
    return static_cast<int>(GetDpiForWindow(state.notification_window->host_hwnd));
  }
  if (state.floating_window->window.dpi > 0) {
    return static_cast<int>(state.floating_window->window.dpi);
  }
  if (HDC hdc = GetDC(nullptr); hdc) {
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : 96;
  }
  return 96;
}

auto get_window_width(int dpi) -> int {
  return scale_for_dpi(NotificationWindow::BASE_WINDOW_WIDTH, dpi);
}

auto get_layout_margin(int dpi) -> int {
  const auto style = resolve_visual_style(dpi);
  return scale_for_dpi(NotificationWindow::BASE_PADDING, dpi) + style.shadow_margin;
}

auto get_host_size(int dpi) -> SIZE {
  const int margin = get_layout_margin(dpi);
  const int spacing = scale_for_dpi(NotificationWindow::BASE_SPACING, dpi);
  const int width = get_window_width(dpi) + margin * 2;
  const int height = scale_for_dpi(NotificationWindow::BASE_MAX_HEIGHT, dpi) *
                         NotificationWindow::MAX_VISIBLE_NOTIFICATIONS +
                     spacing * (NotificationWindow::MAX_VISIBLE_NOTIFICATIONS - 1) + margin * 2;
  return {width, height};
}

auto measure_text_height(NotificationWindow::RenderResources& render_resources,
                         IDWriteFactory7* write_factory, const std::wstring& text, float width)
    -> float {
  if (!write_factory || !render_resources.message_text_format || width <= 0.0f) {
    return 0.0f;
  }

  wil::com_ptr<IDWriteTextLayout> layout;
  const HRESULT hr = write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), render_resources.message_text_format.get(),
      width, 10000.0f, layout.put());
  if (FAILED(hr) || !layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  return metrics.height;
}

auto measure_text_width(NotificationWindow::RenderResources& render_resources,
                        IDWriteFactory7* write_factory, const std::wstring& text,
                        IDWriteTextFormat* format) -> float {
  if (!write_factory || !format || text.empty()) {
    return 0.0f;
  }

  wil::com_ptr<IDWriteTextLayout> layout;
  const HRESULT hr = write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), format, 10000.0f, 10000.0f, layout.put());
  if (FAILED(hr) || !layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  return metrics.width;
}

auto calculate_button_width(Core::State::AppState& state, const std::wstring& label, int max_width)
    -> int {
  const int dpi = get_current_dpi(state);
  const int min_width = scale_for_dpi(NotificationWindow::BASE_BUTTON_WIDTH, dpi);
  const int text_padding = scale_for_dpi(NotificationWindow::BASE_BUTTON_TEXT_PADDING, dpi);
  int width = min_width;

  const auto& render_resources = state.notification_window->render_resources;
  auto* write_factory = state.shared_render_resources->write_factory.get();
  if (render_resources.is_ready && render_resources.button_text_format && write_factory) {
    width = std::max(min_width, static_cast<int>(std::ceil(measure_text_width(
                                    state.notification_window->render_resources, write_factory,
                                    label, render_resources.button_text_format.get()))) +
                                    text_padding * 2);
  }

  return std::min(width, std::max(min_width, max_width));
}

auto measure_message_block(Core::State::AppState& state, const std::wstring& message,
                           float message_text_width) -> float {
  const int font_size = scale_for_dpi(NotificationWindow::BASE_FONT_SIZE, get_current_dpi(state));
  float message_height = static_cast<float>(font_size);
  if (message_text_width <= 0.0f) {
    return message_height;
  }
  if (UI::NotificationWindow::RenderContext::ensure_render_context(state)) {
    auto* write_factory = state.shared_render_resources->write_factory.get();
    message_height =
        std::max(message_height, measure_text_height(state.notification_window->render_resources,
                                                     write_factory, message, message_text_width));
  }
  return message_height;
}

auto normalize_action(std::optional<Core::Notifications::Types::NotificationAction> action)
    -> std::optional<Core::Notifications::Types::NotificationAction> {
  if (!action || action->label.empty()) {
    return std::nullopt;
  }
  return action;
}

auto compute_notification_layout(
    Core::State::AppState& state, const std::wstring& message,
    const std::optional<Core::Notifications::Types::NotificationAction>& action, int card_width)
    -> NotificationWindow::NotificationLayoutMetrics {
  const int dpi = get_current_dpi(state);
  NotificationWindow::NotificationLayoutMetrics metrics{
      .padding = scale_for_dpi(NotificationWindow::BASE_PADDING, dpi),
      .content_padding = scale_for_dpi(NotificationWindow::BASE_CONTENT_PADDING, dpi),
      .column_gap = scale_for_dpi(NotificationWindow::BASE_ACTION_COLUMN_GAP, dpi),
      .title_height = scale_for_dpi(NotificationWindow::BASE_TITLE_HEIGHT, dpi),
      .title_message_gap = scale_for_dpi(NotificationWindow::BASE_TITLE_MESSAGE_GAP, dpi),
      .button_height = scale_for_dpi(NotificationWindow::BASE_BUTTON_HEIGHT, dpi),
  };

  if (action) {
    const int max_width = card_width - metrics.content_padding * 2 - metrics.column_gap;
    metrics.action_width = calculate_button_width(state, action->label, max_width);
  }

  const float message_text_width =
      static_cast<float>(card_width - metrics.content_padding * 2 -
                         (action ? metrics.action_width + metrics.column_gap : 0));
  const float message_height = measure_message_block(state, message, message_text_width);
  const int left_height = metrics.title_height + metrics.title_message_gap +
                          static_cast<int>(std::ceil(message_height));
  metrics.content_height = std::max(left_height, action ? metrics.button_height : 0);
  return metrics;
}

auto measure_card_height(const NotificationWindow::NotificationLayoutMetrics& layout, int dpi)
    -> int {
  const int total_height = layout.padding * 2 + layout.content_height;
  const int min_height = scale_for_dpi(NotificationWindow::BASE_MIN_HEIGHT, dpi);
  const int max_height = scale_for_dpi(NotificationWindow::BASE_MAX_HEIGHT, dpi);
  return std::clamp(total_height, min_height, max_height);
}

auto set_brush_color(ID2D1SolidColorBrush* brush, D2D1_COLOR_F color) -> bool {
  if (!brush) {
    return false;
  }
  brush->SetColor(color);
  return true;
}

auto fill_rounded_rect(NotificationWindow::RenderResources& render_resources,
                       const D2D1_ROUNDED_RECT& rect, D2D1_COLOR_F color) -> void {
  if (!render_resources.device_context ||
      !set_brush_color(render_resources.fill_brush.get(), color)) {
    return;
  }
  render_resources.device_context->FillRoundedRectangle(rect, render_resources.fill_brush.get());
}

auto inset_rounded_rect(const D2D1_ROUNDED_RECT& rect, float inset) -> D2D1_ROUNDED_RECT {
  const auto& bounds = rect.rect;
  const float radius = std::max(0.0f, rect.radiusX - inset);
  return D2D1::RoundedRect(D2D1::RectF(bounds.left + inset, bounds.top + inset,
                                       bounds.right - inset, bounds.bottom - inset),
                           radius, radius);
}

auto draw_stroked_rounded_rect(NotificationWindow::RenderResources& render_resources,
                               const D2D1_ROUNDED_RECT& rect, D2D1_COLOR_F color,
                               float stroke_width) -> void {
  if (!render_resources.device_context || stroke_width <= 0.0f ||
      !set_brush_color(render_resources.stroke_brush.get(), color)) {
    return;
  }

  render_resources.device_context->DrawRoundedRectangle(
      inset_rounded_rect(rect, stroke_width / 2.0f), render_resources.stroke_brush.get(),
      stroke_width);
}

auto expanded_rounded_rect(const D2D1_RECT_F& rect, float spread, float y_offset, float radius)
    -> D2D1_ROUNDED_RECT {
  constexpr float side_spread_scale = 0.2f;
  constexpr float top_spread_scale = 0.1f;
  const float side_spread = spread * side_spread_scale;
  const float top_spread = spread * top_spread_scale;
  const auto expanded = D2D1::RectF(rect.left - side_spread, rect.top - top_spread + y_offset,
                                    rect.right + side_spread, rect.bottom + spread + y_offset);
  return D2D1::RoundedRect(expanded, radius + spread, radius + spread);
}

auto draw_card_shadow(NotificationWindow::RenderResources& render_resources,
                      const D2D1_RECT_F& rect, const NotificationVisualStyle& style, float opacity)
    -> void {
  if (!style.use_system_chrome || style.shadow_margin <= 0 || opacity <= 0.0f) {
    return;
  }

  struct ShadowLayer {
    float spread;
    float y_offset;
    float alpha;
  };

  constexpr std::array<ShadowLayer, 4> layers{{
      {.spread = 1.0f, .y_offset = 1.0f, .alpha = 0.06f},
      {.spread = 3.0f, .y_offset = 2.0f, .alpha = 0.04f},
      {.spread = 6.0f, .y_offset = 4.0f, .alpha = 0.025f},
      {.spread = 10.0f, .y_offset = 7.0f, .alpha = 0.014f},
  }};

  for (const auto& layer : layers) {
    fill_rounded_rect(
        render_resources,
        expanded_rounded_rect(rect, layer.spread, layer.y_offset, style.corner_radius),
        D2D1::ColorF(0.0f, 0.0f, 0.0f, layer.alpha * opacity));
  }
}

auto draw_text(NotificationWindow::RenderResources& render_resources, std::wstring_view text,
               IDWriteTextFormat* format, const D2D1_RECT_F& rect, D2D1_COLOR_F color) -> void {
  if (!render_resources.device_context || !format || text.empty() ||
      !set_brush_color(render_resources.text_brush.get(), color)) {
    return;
  }

  render_resources.device_context->DrawText(text.data(), static_cast<UINT32>(text.length()), format,
                                            rect, render_resources.text_brush.get(),
                                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

auto update_notification_rects(NotificationWindow::Notification& notification) -> void {
  const auto& layout = notification.layout;
  const bool has_action = notification.action.has_value();

  notification.card_rect = {notification.current_pos.x, notification.current_pos.y,
                            notification.current_pos.x + notification.width,
                            notification.current_pos.y + notification.height};

  const int inner_top = notification.card_rect.top + layout.padding;
  const int inner_bottom = notification.card_rect.top + layout.padding + layout.content_height;
  const int content_right = has_action ? notification.card_rect.right - layout.content_padding -
                                             layout.action_width - layout.column_gap
                                       : notification.card_rect.right - layout.content_padding;

  notification.content_rect = {notification.card_rect.left + layout.content_padding, inner_top,
                               content_right, inner_bottom};
  notification.title_rect = {notification.content_rect.left, notification.content_rect.top,
                             notification.content_rect.right,
                             notification.content_rect.top + layout.title_height};
  notification.message_rect = {
      notification.content_rect.left,
      notification.content_rect.top + layout.title_height + layout.title_message_gap,
      notification.content_rect.right, notification.content_rect.bottom};

  if (has_action) {
    const int action_left =
        notification.card_rect.right - layout.content_padding - layout.action_width;
    const int action_top = inner_top + (layout.content_height - layout.button_height) / 2;
    notification.action_rect = {action_left, action_top, action_left + layout.action_width,
                                action_top + layout.button_height};
  } else {
    notification.action_rect = {};
  }
}

auto update_all_notification_rects(Core::State::AppState& state) -> void {
  for (auto& notification : state.notification_window->active_notifications) {
    update_notification_rects(notification);
  }
}

auto draw_action_button(Core::State::AppState& state,
                        const NotificationWindow::Notification& notification) -> void {
  if (!notification.action) {
    return;
  }

  auto& render_resources = state.notification_window->render_resources;
  const int dpi = get_current_dpi(state);
  const auto style = resolve_visual_style(dpi);
  const float hover_radius = style.corner_radius / 2.0f;
  const auto rect = rect_to_d2d(notification.action_rect);
  const auto rounded = D2D1::RoundedRect(rect, hover_radius, hover_radius);
  D2D1_COLOR_F button_bg = notification.colors.hover;
  if (notification.action_hovered) {
    button_bg.a = 1.0f;
  }
  fill_rounded_rect(render_resources, rounded, color_with_opacity(button_bg, notification.opacity));
  if (notification.action_hovered) {
    draw_stroked_rounded_rect(render_resources, rounded,
                              color_with_opacity(notification.colors.text, notification.opacity),
                              stroke_width_for_dpi(dpi));
  }

  draw_text(render_resources, notification.action->label, render_resources.button_text_format.get(),
            rect, color_with_opacity(notification.colors.text, notification.opacity));
}

auto draw_notification(Core::State::AppState& state,
                       const NotificationWindow::Notification& notification) -> void {
  auto& render_resources = state.notification_window->render_resources;
  if (notification.opacity <= 0.0f ||
      notification.state == NotificationWindow::NotificationAnimState::Done) {
    return;
  }

  const int dpi = get_current_dpi(state);
  const auto style = resolve_visual_style(dpi);
  const D2D1_RECT_F card_rect = rect_to_d2d(notification.card_rect);

  const auto card_rounded = D2D1::RoundedRect(card_rect, style.corner_radius, style.corner_radius);
  draw_card_shadow(render_resources, card_rect, style, notification.opacity);
  fill_rounded_rect(render_resources, card_rounded,
                    color_with_opacity(notification.colors.background, notification.opacity));
  if (style.border_width > 0.0f) {
    draw_stroked_rounded_rect(render_resources, card_rounded,
                              color_with_opacity(style.border_color, notification.opacity),
                              style.border_width);
  }

  draw_text(render_resources, notification.title, render_resources.title_text_format.get(),
            rect_to_d2d(notification.title_rect),
            color_with_opacity(notification.colors.text, notification.opacity));
  draw_text(render_resources, notification.message, render_resources.message_text_format.get(),
            rect_to_d2d(notification.message_rect),
            color_with_opacity(notification.colors.text, notification.opacity));
  draw_action_button(state, notification);
}

auto present_render_context(Core::State::AppState& state) -> void {
  auto& render_resources = state.notification_window->render_resources;
  if (!render_resources.swap_chain) {
    return;
  }

  const HRESULT hr = render_resources.swap_chain->Present(0, 0);
  if (FAILED(hr)) {
    Logger().error("Notification present error: 0x{:X}", hr);
  }
}

auto paint_notifications(Core::State::AppState& state) -> void {
  auto& render_resources = state.notification_window->render_resources;
  if (state.notification_window->active_notifications.empty() ||
      !UI::NotificationWindow::RenderContext::ensure_render_context(state) ||
      render_resources.is_rendering) {
    return;
  }

  render_resources.is_rendering = true;
  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
  render_resources.device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

  for (const auto& notification : state.notification_window->active_notifications) {
    draw_notification(state, notification);
  }

  const HRESULT hr = render_resources.device_context->EndDraw();
  render_resources.is_rendering = false;

  if (hr == D2DERR_RECREATE_TARGET) {
    UI::NotificationWindow::RenderContext::cleanup_render_context(state);
    Logger().warn("Notification render target needs recreation");
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Notification paint error: 0x{:X}", hr);
    return;
  }

  present_render_context(state);
}

auto request_repaint(Core::State::AppState& state) -> void {
  if (state.notification_window->host_hwnd &&
      IsWindowVisible(state.notification_window->host_hwnd)) {
    InvalidateRect(state.notification_window->host_hwnd, nullptr, FALSE);
  }
}

}  // namespace UI::NotificationWindow::Painter
