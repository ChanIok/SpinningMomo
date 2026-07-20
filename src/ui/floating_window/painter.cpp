module;

module UI.FloatingWindow.Painter;

import std;
import Core.State;
import Core.Commands;
import Core.Commands.Types;
import UI.SharedRenderResources.State;
import UI.FloatingWindow.Layout;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Types;
import UI.FloatingWindow.RenderContext;
import Features.Settings.Menu;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <windows.h>;

namespace UI::FloatingWindow::Painter {

// 列数据结构：包含原始索引和项指针
struct ColumnItems {
  std::vector<size_t> indices;                             // 在原数组中的索引（用于 hover 判断）
  std::vector<const UI::FloatingWindow::MenuItem*> items;  // 项指针
};

// 列绘制参数
struct ColumnDrawParams {
  float x_left;
  float x_right;
  size_t scroll_offset;
  size_t max_visible;
};

constexpr float kWidthCacheScale = 10.0f;
constexpr float kFontCacheScale = 100.0f;
constexpr size_t kMaxTextMeasureCacheEntries = 256;
constexpr size_t kMaxAdjustedFormatEntries = 32;

auto to_cache_key(float value, float scale) -> int {
  return static_cast<int>(std::lround(value * scale));
}

auto find_cached_font_key(const UI::FloatingWindow::RenderResources& d2d, std::wstring_view text,
                          int width_key, int base_font_key) -> std::optional<int> {
  for (const auto& entry : d2d.text_measure_cache) {
    if (entry.width_key == width_key && entry.base_font_key == base_font_key &&
        entry.text == text) {
      return entry.resolved_font_key;
    }
  }
  return std::nullopt;
}

auto store_text_measure_cache(UI::FloatingWindow::RenderResources& d2d, std::wstring_view text,
                              int width_key, int base_font_key, int resolved_font_key) -> void {
  if (d2d.text_measure_cache.size() >= kMaxTextMeasureCacheEntries) {
    d2d.text_measure_cache.clear();
  }

  d2d.text_measure_cache.push_back(UI::FloatingWindow::TextMeasureCacheEntry{
      .text = std::wstring(text),
      .width_key = width_key,
      .base_font_key = base_font_key,
      .resolved_font_key = resolved_font_key,
  });
}

auto get_or_create_adjusted_text_format(UI::FloatingWindow::RenderResources& d2d,
                                        IDWriteFactory7* write_factory, int font_key)
    -> IDWriteTextFormat* {
  if (auto it = d2d.adjusted_text_formats.find(font_key); it != d2d.adjusted_text_formats.end()) {
    return it->second.get();
  }

  if (d2d.adjusted_text_formats.size() >= kMaxAdjustedFormatEntries) {
    d2d.adjusted_text_formats.clear();
  }

  const float font_size = static_cast<float>(font_key) / kFontCacheScale;
  auto text_format =
      UI::FloatingWindow::RenderContext::create_text_format_with_size(write_factory, font_size);
  if (!text_format) {
    return nullptr;
  }

  auto* text_format_ptr = text_format.get();
  d2d.adjusted_text_formats.emplace(font_key, std::move(text_format));
  return text_format_ptr;
}

// 按类别分组菜单项
auto group_items_by_column(const std::vector<UI::FloatingWindow::MenuItem>& items)
    -> std::tuple<ColumnItems, ColumnItems, ColumnItems> {
  ColumnItems ratio, resolution, feature;

  for (size_t i = 0; i < items.size(); ++i) {
    switch (items[i].category) {
      case UI::FloatingWindow::MenuItemCategory::AspectRatio:
        ratio.indices.push_back(i);
        ratio.items.push_back(&items[i]);
        break;
      case UI::FloatingWindow::MenuItemCategory::Resolution:
        resolution.indices.push_back(i);
        resolution.items.push_back(&items[i]);
        break;
      case UI::FloatingWindow::MenuItemCategory::Feature:
        feature.indices.push_back(i);
        feature.items.push_back(&items[i]);
        break;
    }
  }

  return {ratio, resolution, feature};
}

// 绘制单个列
auto draw_single_column(Core::State::AppState& state, const D2D1_RECT_F& rect,
                        const ColumnItems& column, const ColumnDrawParams& params) -> void {
  const auto& render = state.floating_window->layout;
  float y = rect.top + static_cast<float>(render.title_height + render.separator_height);

  const size_t start_index = params.scroll_offset;
  const size_t end_index = std::min(start_index + params.max_visible, column.items.size());

  // 绘制可见项
  for (size_t i = start_index; i < end_index; ++i) {
    const auto& item = *column.items[i];
    const size_t original_index = column.indices[i];

    D2D1_RECT_F item_rect = UI::FloatingWindow::make_d2d_rect(
        params.x_left, y, params.x_right, y + static_cast<float>(render.item_height));

    const bool is_hovered =
        (static_cast<int>(original_index) == state.floating_window->ui.hover_index);
    draw_single_item(state, item, item_rect, is_hovered);

    y += static_cast<float>(render.item_height);
  }
}

// 绘制滚动条指示器
auto draw_scroll_indicator(const Core::State::AppState& state, const D2D1_RECT_F& column_rect,
                           size_t total_items, size_t scroll_offset, bool is_hovered,
                           bool is_last_column) -> void {
  const auto& render = state.floating_window->layout;
  if (!is_hovered || total_items <= static_cast<size_t>(render.max_visible_rows)) {
    return;  // 不需要显示滚动条
  }

  const auto& d2d = state.floating_window->render_resources;

  // 计算轨道高度
  const float track_height = static_cast<float>(render.item_height * render.max_visible_rows);
  const float track_top =
      column_rect.top + static_cast<float>(render.title_height + render.separator_height);

  // 分页模式：计算总页数和当前页号
  const int page_size = render.max_visible_rows;
  const int total_pages = (static_cast<int>(total_items) + page_size - 1) / page_size;
  const int current_page = static_cast<int>(scroll_offset) / page_size;

  // 滑块高度 = 轨道高度 / 总页数
  const float thumb_height = track_height / static_cast<float>(total_pages);

  // 滑块位置：根据当前页号分布在轨道上
  const float thumb_top =
      (total_pages > 1)
          ? track_top + (track_height - thumb_height) *
                            (static_cast<float>(current_page) / static_cast<float>(total_pages - 1))
          : track_top;

  // 滚动条宽度和位置（与分隔线右边界对齐，最后一列除外）
  const float indicator_width = static_cast<float>(render.scroll_indicator_width);
  const float indicator_right =
      is_last_column ? column_rect.right - 1.0f
                     : column_rect.right + static_cast<float>(render.separator_height);
  const float indicator_left = indicator_right - indicator_width;

  // 绘制滑块
  D2D1_RECT_F thumb_rect = UI::FloatingWindow::make_d2d_rect(
      indicator_left, thumb_top, indicator_right, thumb_top + thumb_height);
  d2d.device_context->FillRectangle(thumb_rect, d2d.scroll_indicator_brush.get());
}

// 主绘制函数实现
auto paint(Core::State::AppState& state, HWND hwnd, const RECT& client_rect) -> void {
  auto& d2d = state.floating_window->render_resources;

  if (!d2d.is_initialized || !d2d.device_context) {
    return;
  }

  // 先处理字体更新（如果需要）
  if (d2d.needs_font_update) {
    if (!UI::FloatingWindow::RenderContext::update_text_format_if_needed(state)) {
      return;  // 字体更新失败，无法继续绘制
    }
  }

  if (d2d.is_rendering) {
    return;
  }

  d2d.is_rendering = true;

  d2d.device_context->BeginDraw();

  // 清空背景为完全透明
  d2d.device_context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  // 全局设置替换混合模式，避免所有颜色叠加
  d2d.device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

  const auto rect_f = UI::FloatingWindow::rect_to_d2d(client_rect);

  // 绘制各个部分
  draw_background(state, rect_f);
  draw_title_bar(state, rect_f);
  draw_separators(state, rect_f);
  draw_items(state, rect_f);

  HRESULT hr = d2d.device_context->EndDraw();

  // 处理设备丢失等错误
  if (hr == D2DERR_RECREATE_TARGET) {
    // composition back buffer 已失效时，直接重建整套后端比局部修补更可靠，
    // 上层布局和交互状态保持不动。
    UI::FloatingWindow::RenderContext::initialize_render_context(state, hwnd);
    d2d.is_rendering = false;
    return;
  }

  d2d.is_rendering = false;

  // 提交到 composition swap chain；DWM 会在下一轮合成中显示最新内容。
  if (SUCCEEDED(hr) && d2d.swap_chain) {
    d2d.swap_chain->Present(0, 0);
  }
}

// 绘制背景
auto draw_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.floating_window->render_resources;
  // 使用半透明白色背景
  d2d.device_context->FillRectangle(rect, d2d.background_brush.get());
}

// 绘制关闭按钮
auto draw_close_button(const Core::State::AppState& state, const D2D1_RECT_F& title_rect) -> void {
  const auto& d2d = state.floating_window->render_resources;
  const auto& render = state.floating_window->layout;

  // 计算按钮尺寸（正方形，与标题栏高度一致）
  const float button_size = static_cast<float>(render.title_height);

  // 计算按钮位置（右上角）
  const float x = title_rect.right - button_size;
  const float y = title_rect.top;

  // 创建按钮区域矩形
  const D2D1_RECT_F button_rect = D2D1::RectF(x, y, x + button_size, y + button_size);

  // 绘制悬停背景（如果需要）
  if (state.floating_window->ui.close_button_hovered) {
    d2d.device_context->FillRectangle(button_rect, d2d.hover_brush.get());
  }

  // 计算"X"图标尺寸和位置
  const float icon_margin = button_size * 0.35f;  // 边距
  const float icon_size = button_size - 2 * icon_margin;

  const float icon_left = x + icon_margin;
  const float icon_top = y + icon_margin;
  const float icon_right = icon_left + icon_size;
  const float icon_bottom = icon_top + icon_size;

  // 绘制"X"图标
  const float pen_width = 1.5f;
  d2d.device_context->DrawLine(D2D1::Point2F(icon_left, icon_top),
                               D2D1::Point2F(icon_right, icon_bottom), d2d.text_brush.get(),
                               pen_width, nullptr);

  d2d.device_context->DrawLine(D2D1::Point2F(icon_right, icon_top),
                               D2D1::Point2F(icon_left, icon_bottom), d2d.text_brush.get(),
                               pen_width, nullptr);
}

// 绘制标题栏
auto draw_title_bar(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.floating_window->render_resources;
  const auto& render = state.floating_window->layout;

  // 绘制标题栏背景
  D2D1_RECT_F title_rect = UI::FloatingWindow::make_d2d_rect(
      rect.left, rect.top, rect.right, rect.top + static_cast<float>(render.title_height));
  d2d.device_context->FillRectangle(title_rect, d2d.title_brush.get());

  // 绘制标题文本（保持完全不透明）
  D2D1_RECT_F text_rect = UI::FloatingWindow::make_d2d_rect(
      rect.left + static_cast<float>(render.text_padding), rect.top, rect.right,
      rect.top + static_cast<float>(render.title_height));

  d2d.device_context->DrawText(L"SpinningMomo",
                               12,  // 文本长度
                               d2d.text_format.get(), text_rect, d2d.text_brush.get());

  // 绘制关闭按钮
  draw_close_button(state, title_rect);
}

// 绘制分隔线
auto draw_separators(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.floating_window->render_resources;
  const auto& render = state.floating_window->layout;

  // 使用简单的列边界计算
  const auto bounds = UI::FloatingWindow::Layout::get_column_bounds(state);

  // 绘制水平分隔线（使用半透明画刷）
  D2D1_RECT_F h_sep_rect = UI::FloatingWindow::make_d2d_rect(
      rect.left, rect.top + static_cast<float>(render.title_height), rect.right,
      rect.top + static_cast<float>(render.title_height + render.separator_height));
  d2d.device_context->FillRectangle(h_sep_rect, d2d.separator_brush.get());

  // 绘制垂直分隔线1（使用半透明画刷）
  D2D1_RECT_F v_sep_rect1 = UI::FloatingWindow::make_d2d_rect(
      static_cast<float>(bounds.ratio_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.ratio_column_right + render.separator_height), rect.bottom);
  d2d.device_context->FillRectangle(v_sep_rect1, d2d.separator_brush.get());

  // 绘制垂直分隔线2（使用半透明画刷）
  D2D1_RECT_F v_sep_rect2 = UI::FloatingWindow::make_d2d_rect(
      static_cast<float>(bounds.resolution_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.resolution_column_right + render.separator_height), rect.bottom);
  d2d.device_context->FillRectangle(v_sep_rect2, d2d.separator_brush.get());
}

// 绘制所有菜单项
auto draw_items(Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render = state.floating_window->layout;
  const auto& ui = state.floating_window->ui;
  const auto& items = state.floating_window->data.menu_items;
  const auto bounds = UI::FloatingWindow::Layout::get_column_bounds(state);

  // 按类别分组
  auto [ratio_col, resolution_col, feature_col] = group_items_by_column(items);

  const size_t max_visible = static_cast<size_t>(render.max_visible_rows);

  // 绘制比例列
  draw_single_column(state, rect, ratio_col,
                     {.x_left = rect.left,
                      .x_right = static_cast<float>(bounds.ratio_column_right),
                      .scroll_offset = ui.ratio_scroll_offset,
                      .max_visible = max_visible});

  // 绘制分辨率列
  draw_single_column(
      state, rect, resolution_col,
      {.x_left = static_cast<float>(bounds.ratio_column_right + render.separator_height),
       .x_right = static_cast<float>(bounds.resolution_column_right),
       .scroll_offset = ui.resolution_scroll_offset,
       .max_visible = max_visible});

  // 绘制功能列
  draw_single_column(
      state, rect, feature_col,
      {.x_left = static_cast<float>(bounds.resolution_column_right + render.separator_height),
       .x_right = rect.right,
       .scroll_offset = ui.feature_scroll_offset,
       .max_visible = max_visible});

  // 比例列滚动条
  D2D1_RECT_F ratio_column_rect = UI::FloatingWindow::make_d2d_rect(
      rect.left, rect.top, static_cast<float>(bounds.ratio_column_right), rect.bottom);
  draw_scroll_indicator(state, ratio_column_rect, ratio_col.items.size(), ui.ratio_scroll_offset,
                        ui.hovered_column == 0, false);

  // 分辨率列滚动条
  D2D1_RECT_F resolution_column_rect = UI::FloatingWindow::make_d2d_rect(
      static_cast<float>(bounds.ratio_column_right + render.separator_height), rect.top,
      static_cast<float>(bounds.resolution_column_right), rect.bottom);
  draw_scroll_indicator(state, resolution_column_rect, resolution_col.items.size(),
                        ui.resolution_scroll_offset, ui.hovered_column == 1, false);

  // 功能列滚动条
  D2D1_RECT_F feature_column_rect = UI::FloatingWindow::make_d2d_rect(
      static_cast<float>(bounds.resolution_column_right + render.separator_height), rect.top,
      rect.right, rect.bottom);
  draw_scroll_indicator(state, feature_column_rect, feature_col.items.size(),
                        ui.feature_scroll_offset, ui.hovered_column == 2, true);
}

auto is_item_selected(const UI::FloatingWindow::MenuItem& item,
                      const Core::State::AppState& app_state) -> bool {
  switch (item.category) {
    case UI::FloatingWindow::MenuItemCategory::AspectRatio:
      return item.index == static_cast<int>(app_state.floating_window->ui.current_ratio_index);
    case UI::FloatingWindow::MenuItemCategory::Resolution:
      return item.index == static_cast<int>(app_state.floating_window->ui.current_resolution_index);
    case UI::FloatingWindow::MenuItemCategory::Feature:
      return Core::Commands::is_toggle_on(app_state, item.action_id);
    default:
      return false;
  }
}

// 绘制单个菜单项
auto draw_single_item(Core::State::AppState& state, const UI::FloatingWindow::MenuItem& item,
                      const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  auto& d2d = state.floating_window->render_resources;
  const auto& render = state.floating_window->layout;
  const int indicator_width = render.indicator_width;

  // 绘制悬停背景
  if (is_hovered) {
    d2d.device_context->FillRectangle(item_rect, d2d.hover_brush.get());
  }

  // 绘制选中指示器（保持完全不透明）
  const bool is_selected = is_item_selected(item, state);
  if (is_selected) {
    float indicator_left = item_rect.left;
    if (item.category == UI::FloatingWindow::MenuItemCategory::AspectRatio) {
      // 比例列贴着窗口左沿，需避开 DWM 覆盖在客户区上的系统描边。
      indicator_left +=
          static_cast<float>(state.floating_window->window.visible_frame_border_thickness);
    }
    D2D1_RECT_F indicator_rect = UI::FloatingWindow::make_d2d_rect(
        indicator_left, item_rect.top, indicator_left + static_cast<float>(indicator_width),
        item_rect.bottom);
    ID2D1SolidColorBrush* indicator_brush = d2d.indicator_brush.get();
    if (item.category == UI::FloatingWindow::MenuItemCategory::Feature &&
        item.action_id == "recording.toggle" && d2d.recording_indicator_brush) {
      indicator_brush = d2d.recording_indicator_brush.get();
    }
    if (indicator_brush) {
      d2d.device_context->FillRectangle(indicator_rect, indicator_brush);
    }
  }

  // 绘制文本（保持完全不透明）
  D2D1_RECT_F text_rect = UI::FloatingWindow::make_d2d_rect(
      item_rect.left + static_cast<float>(render.text_padding + indicator_width), item_rect.top,
      item_rect.right - static_cast<float>(render.text_padding / 2), item_rect.bottom);
  const auto draw_default_text = [&]() -> void {
    d2d.device_context->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                 d2d.text_format.get(), text_rect, d2d.text_brush.get());
  };
  auto* write_factory = state.shared_render_resources->write_factory.get();

  // 计算可用于文本的宽度
  const float available_width = text_rect.right - text_rect.left;

  // 如果文本为空或宽度无效，则直接使用默认字体绘制
  if (item.text.empty() || available_width <= 0.0f || !d2d.text_format || !write_factory) {
    draw_default_text();
    return;
  }

  const int width_key = to_cache_key(available_width, kWidthCacheScale);
  const int base_font_key = to_cache_key(render.font_size, kFontCacheScale);

  int resolved_font_key = base_font_key;
  if (const auto cached_font_key = find_cached_font_key(d2d, item.text, width_key, base_font_key)) {
    resolved_font_key = *cached_font_key;
  } else {
    float text_width = UI::FloatingWindow::RenderContext::measure_text_width(
        item.text, d2d.text_format.get(), write_factory);

    if (text_width > available_width) {
      float adjusted_font_size = render.font_size;

      for (adjusted_font_size -= UI::FloatingWindow::LayoutConfig::FONT_SIZE_STEP;
           adjusted_font_size >= UI::FloatingWindow::LayoutConfig::MIN_FONT_SIZE;
           adjusted_font_size -= UI::FloatingWindow::LayoutConfig::FONT_SIZE_STEP) {
        const float clamped_font_size =
            std::max(adjusted_font_size, UI::FloatingWindow::LayoutConfig::MIN_FONT_SIZE);

        const int adjusted_font_key = to_cache_key(clamped_font_size, kFontCacheScale);
        auto* adjusted_text_format =
            get_or_create_adjusted_text_format(d2d, write_factory, adjusted_font_key);
        if (!adjusted_text_format) {
          break;
        }

        text_width = UI::FloatingWindow::RenderContext::measure_text_width(
            item.text, adjusted_text_format, write_factory);
        if (text_width <= available_width) {
          resolved_font_key = adjusted_font_key;
          break;
        }
      }
    }

    store_text_measure_cache(d2d, item.text, width_key, base_font_key, resolved_font_key);
  }

  if (resolved_font_key == base_font_key) {
    draw_default_text();
    return;
  }

  if (auto* adjusted_text_format =
          get_or_create_adjusted_text_format(d2d, write_factory, resolved_font_key)) {
    d2d.device_context->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                 adjusted_text_format, text_rect, d2d.text_brush.get());
    return;
  }

  // 缓存命中但创建失败时，回退默认字体绘制
  draw_default_text();
}

}  // namespace UI::FloatingWindow::Painter
