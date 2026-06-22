module;

module UI.PhotographyPanel.Painter;

import std;
import Core.State;
import Core.I18n.State;
import Features.Photography.LongExposure;
import Features.Photography.State;
import UI.FloatingWindow.State;
import UI.PhotographyPanel.RenderContext;
import UI.PhotographyPanel.State;
import Utils.Logger;
import Utils.String;
import <d2d1_3.h>;
import <windows.h>;

namespace UI::PhotographyPanel::Painter {

auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

// 根据帧数返回 i18n 标签：0 帧显示"关"，其余用格式化字符串显示帧数
auto long_exposure_label(const Core::State::AppState& state, int frames) -> std::wstring {
  if (frames <= 0) {
    return Utils::String::FromUtf8(state.i18n->texts["photography.long_exposure_off"]);
  }
  return Utils::String::FromUtf8(std::vformat(state.i18n->texts["photography.long_exposure_frames"],
                                              std::make_format_args(frames)));
}

// 面板沿用浮窗的标题、字体和 item 节奏，只保留滑块自身的几何尺寸定义
auto compute_panel_layout(const Core::State::AppState& state) -> State::PanelLayoutMetrics {
  const auto& floating_layout = state.floating_window->layout;
  const int title_height = floating_layout.title_height;
  const int item_height = floating_layout.item_height;
  const int title_text_padding = floating_layout.text_padding;
  const int content_padding = floating_layout.text_padding * 2;

  const int label_top = title_height + item_height / 2;
  const int label_bottom = label_top + item_height;
  const int slider_row_top = label_bottom;
  const int slider_row_bottom = slider_row_top + item_height;
  const int slider_center_y = slider_row_top + item_height / 2;

  return State::PanelLayoutMetrics{
      .window_size = {State::kPanelWidth, title_height + item_height * 3},
      .title_rect = {0, 0, State::kPanelWidth, title_height},
      .title_text_rect = {title_text_padding, 0, State::kPanelWidth - title_text_padding,
                          title_height},
      .label_rect = {content_padding, label_top, State::kPanelWidth - content_padding,
                     label_bottom},
      .slider_row_rect = {content_padding, slider_row_top, State::kPanelWidth - content_padding,
                          slider_row_bottom},
      .slider_rect = {content_padding, slider_center_y - State::kSliderTrackHalfHeight,
                      State::kPanelWidth - content_padding,
                      slider_center_y + State::kSliderTrackHalfHeight},
  };
}

// 将帧数反算为滑块 X 坐标：先 snap 到最近档位，再线性映射到像素位置
auto shutter_to_x(const RECT& rect, int frames) -> float {
  const int nearest = Features::Photography::LongExposure::nearest_frame_stop(frames);
  const auto stops = Features::Photography::LongExposure::frame_stops();
  const auto it = std::ranges::find(stops, nearest);
  const auto index = it == stops.end() ? 0 : static_cast<int>(it - stops.begin());
  const int max_index = static_cast<int>(stops.size()) - 1;
  const float ratio =
      max_index == 0 ? 0.0f : static_cast<float>(index) / static_cast<float>(max_index);
  return static_cast<float>(rect.left) +
         ratio * static_cast<float>(std::max<LONG>(1, rect.right - rect.left));
}

auto draw_text_line(State::RenderResources& render_resources, std::wstring_view text,
                    const D2D1_RECT_F& rect) -> void {
  if (!render_resources.device_context || !render_resources.text_format ||
      !render_resources.text_brush) {
    return;
  }

  render_resources.device_context->DrawTextW(
      text.data(), static_cast<UINT32>(text.length()), render_resources.text_format.get(), rect,
      render_resources.text_brush.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

// 旋钮 hover 时只补一圈描边，沿用通知按钮那种轻量反馈，不改变滑块整体气质
auto draw_slider(const Core::State::AppState& state, State::RenderResources& render_resources,
                 const RECT& rect, float knob_x) -> void {
  if (!render_resources.device_context || !render_resources.track_brush ||
      !render_resources.knob_brush) {
    return;
  }

  const float center_y = static_cast<float>(rect.top + rect.bottom) * 0.5f;
  render_resources.device_context->DrawLine(D2D1::Point2F(static_cast<float>(rect.left), center_y),
                                            D2D1::Point2F(static_cast<float>(rect.right), center_y),
                                            render_resources.track_brush.get(), 3.0f);
  render_resources.device_context->FillEllipse(
      D2D1::Ellipse(D2D1::Point2F(knob_x, center_y), State::kSliderKnobRadius,
                    State::kSliderKnobRadius),
      render_resources.knob_brush.get());

  if (state.photography_panel->knob_hovered && render_resources.text_brush) {
    render_resources.device_context->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(knob_x, center_y), State::kSliderKnobRadius,
                      State::kSliderKnobRadius),
        render_resources.text_brush.get(), State::kSliderKnobHoverStrokeWidth);
  }
}

// D2D 绘制整个面板：背景 → 标题栏 → 长曝光控件
auto paint(Core::State::AppState& state, HWND hwnd) -> void {
  if (!UI::PhotographyPanel::RenderContext::ensure_render_context(state)) {
    return;
  }

  auto& panel = *state.photography_panel;
  auto& render_resources = panel.render_resources;
  if (render_resources.is_rendering || !render_resources.device_context) {
    return;
  }

  RECT client_rect{};
  GetClientRect(hwnd, &client_rect);
  const auto layout = compute_panel_layout(state);

  render_resources.is_rendering = true;
  render_resources.device_context->BeginDraw();
  render_resources.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
  render_resources.device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

  render_resources.device_context->FillRectangle(rect_to_d2d(client_rect),
                                                 render_resources.background_brush.get());

  render_resources.device_context->FillRectangle(rect_to_d2d(layout.title_rect),
                                                 render_resources.title_brush.get());

  const auto title = Utils::String::FromUtf8(state.i18n->texts["menu.photography_toggle"]);
  draw_text_line(render_resources, title, rect_to_d2d(layout.title_text_rect));

  const int frames = state.photography->shutter_frames.load(std::memory_order_acquire);
  const auto value_label = long_exposure_label(state, frames);
  draw_text_line(render_resources, value_label, rect_to_d2d(layout.label_rect));

  draw_slider(state, render_resources, layout.slider_rect,
              shutter_to_x(layout.slider_rect, frames));

  const HRESULT hr = render_resources.device_context->EndDraw();
  render_resources.is_rendering = false;

  if (hr == D2DERR_RECREATE_TARGET) {
    UI::PhotographyPanel::RenderContext::cleanup_render_context(state);
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Photography panel paint error: 0x{:X}", hr);
    return;
  }

  if (render_resources.swap_chain) {
    render_resources.swap_chain->Present(0, 0);
  }
}

}  // namespace UI::PhotographyPanel::Painter
