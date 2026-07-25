#include "ui/floating_window/layout.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"

#include "core/state/app_state.hpp"
#include "features/settings/state.hpp"
#include "ui/floating_window/state.hpp"

namespace ui::floating_window::layout {

auto calculate_layout_config(const core::AppState& state, UINT dpi)
    -> ui::floating_window::LayoutConfig {
  const auto& settings = state.settings->raw;
  const auto& layout_settings = settings.ui.floating_window_layout;
  const double scale = static_cast<double>(dpi) / 96.0;

  ui::floating_window::LayoutConfig layout;

  // 直接从配置计算实际渲染尺寸
  layout.item_height = static_cast<int>(layout_settings.base_item_height * scale);
  layout.title_height = static_cast<int>(layout_settings.base_title_height * scale);
  layout.separator_height = static_cast<int>(layout_settings.base_separator_height * scale);
  layout.font_size = static_cast<float>(layout_settings.base_font_size * scale);
  layout.text_padding = static_cast<int>(layout_settings.base_text_padding * scale);
  layout.indicator_width = static_cast<int>(layout_settings.base_indicator_width * scale);
  layout.ratio_column_width = static_cast<int>(layout_settings.base_ratio_column_width * scale);
  layout.resolution_column_width =
      static_cast<int>(layout_settings.base_resolution_column_width * scale);
  // 设置项配置名仍沿用历史字段名，内部状态统一收敛到 feature column。
  layout.feature_column_width =
      static_cast<int>(layout_settings.base_settings_column_width * scale);
  layout.scroll_indicator_width =
      static_cast<int>(layout_settings.base_scroll_indicator_width * scale);
  layout.max_visible_rows = std::max(layout_settings.max_visible_rows, 1);
  return layout;
}

auto calculate_window_size(const ui::floating_window::LayoutConfig& layout) -> SIZE {
  const int total_width =
      layout.ratio_column_width + layout.resolution_column_width + layout.feature_column_width;
  const int window_height =
      layout.title_height + layout.separator_height + layout.item_height * layout.max_visible_rows;

  return {total_width, window_height};
}

auto calculate_window_metrics(const core::AppState& state, UINT dpi)
    -> ui::floating_window::WindowMetrics {
  auto layout = calculate_layout_config(state, dpi);
  return ui::floating_window::WindowMetrics{
      .layout = std::move(layout),
      .size = calculate_window_size(layout),
  };
}

auto calculate_center_position(const SIZE& window_size) -> POINT {
  // 获取主显示器工作区
  RECT work_area{};
  if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
    return {0, 0};  // 失败时返回原点
  }

  // 计算窗口位置（屏幕中央）
  const int x_pos = (work_area.right - work_area.left - window_size.cx) / 2;
  const int y_pos = (work_area.bottom - work_area.top - window_size.cy) / 2;

  return {x_pos, y_pos};
}

auto get_item_index_from_point(const core::AppState& state, int x, int y) -> int {
  const auto& render = state.floating_window->layout;
  const auto& items = state.floating_window->data.menu_items;
  const auto& ui = state.floating_window->ui;

  // 检查是否在标题栏或分隔线区域
  if (y < render.title_height + render.separator_height) {
    return -1;
  }

  const auto bounds = get_column_bounds(state);

  // 确定点击的是哪一列
  ui::floating_window::MenuItemCategory target_category;
  size_t scroll_offset = 0;

  if (x < bounds.ratio_column_right) {
    target_category = ui::floating_window::MenuItemCategory::AspectRatio;
    scroll_offset = ui.ratio_scroll_offset;
  } else if (x < bounds.resolution_column_right) {
    target_category = ui::floating_window::MenuItemCategory::Resolution;
    scroll_offset = ui.resolution_scroll_offset;
  } else {
    // 功能列的特殊处理
    return get_feature_item_index(state, y);
  }

  // 处理比例和分辨率列
  size_t visible_index = 0;
  int item_y = render.title_height + render.separator_height;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    if (item.category == target_category) {
      // 翻页模式下跳过不可见项
      if (visible_index < scroll_offset) {
        visible_index++;
        continue;
      }

      if (y >= item_y && y < item_y + render.item_height) {
        return static_cast<int>(i);
      }
      item_y += render.item_height;
      visible_index++;
    }
  }

  return -1;
}

auto count_items_per_column(const std::vector<ui::floating_window::MenuItem>& items)
    -> ColumnCounts {
  ColumnCounts counts;

  for (const auto& item : items) {
    switch (item.category) {
      case ui::floating_window::MenuItemCategory::AspectRatio:
        ++counts.ratio_count;
        break;
      case ui::floating_window::MenuItemCategory::Resolution:
        ++counts.resolution_count;
        break;
      case ui::floating_window::MenuItemCategory::Feature:
        ++counts.feature_count;
        break;
    }
  }

  return counts;
}

auto get_column_bounds(const core::AppState& state) -> ColumnBounds {
  const auto& render = state.floating_window->layout;
  const int ratio_column_right = render.ratio_column_width;
  const int resolution_column_right = ratio_column_right + render.resolution_column_width;
  const int feature_column_left = resolution_column_right + render.separator_height;

  return {ratio_column_right, resolution_column_right, feature_column_left};
}

auto get_feature_item_index(const core::AppState& state, int y) -> int {
  const auto& render = state.floating_window->layout;
  const auto& items = state.floating_window->data.menu_items;
  const auto& ui = state.floating_window->ui;

  const size_t scroll_offset = ui.feature_scroll_offset;

  size_t visible_index = 0;
  int feature_y = render.title_height + render.separator_height;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];

    // 判断是否为功能项
    if (item.category == ui::floating_window::MenuItemCategory::Feature) {
      // 翻页模式下跳过不可见项
      if (visible_index < scroll_offset) {
        visible_index++;
        continue;
      }

      if (y >= feature_y && y < feature_y + render.item_height) {
        return static_cast<int>(i);
      }
      feature_y += render.item_height;
      visible_index++;
    }
  }
  return -1;
}

}  // namespace ui::floating_window::layout
