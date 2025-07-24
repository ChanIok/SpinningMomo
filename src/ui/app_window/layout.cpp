module;

#include <dwmapi.h>
#include <windows.h>

module UI.AppWindow.Layout;

import std;
import Core.State;
import UI.AppWindow.State;
import Features.Settings.State;
import Features.Settings.Types;
import Utils.Logger;

namespace UI::AppWindow::Layout {

auto update_layout(Core::State::AppState& state) -> void {
  const auto& settings = state.settings->config;
  const auto& layout_settings = settings.ui.app_window_layout;
  const UINT dpi = state.app_window->window.dpi;
  const double scale = static_cast<double>(dpi) / 96.0;

  auto& layout = state.app_window->layout;

  // 直接从配置计算实际渲染尺寸
  layout.item_height = static_cast<int>(layout_settings.base_item_height * scale);
  layout.title_height = static_cast<int>(layout_settings.base_title_height * scale);
  layout.separator_height = static_cast<int>(layout_settings.base_separator_height * scale);
  layout.font_size = static_cast<float>(layout_settings.base_font_size * scale);
  layout.text_padding = static_cast<int>(layout_settings.base_text_padding * scale);
  layout.indicator_width = static_cast<int>(layout_settings.base_indicator_width * scale);
  layout.ratio_indicator_width =
      static_cast<int>(layout_settings.base_ratio_indicator_width * scale);
  layout.ratio_column_width = static_cast<int>(layout_settings.base_ratio_column_width * scale);
  layout.resolution_column_width =
      static_cast<int>(layout_settings.base_resolution_column_width * scale);
  layout.settings_column_width =
      static_cast<int>(layout_settings.base_settings_column_width * scale);
}

auto calculate_window_size(const Core::State::AppState& state) -> SIZE {
  const auto& render = state.app_window->layout;
  const int total_width =
      render.ratio_column_width + render.resolution_column_width + render.settings_column_width;
  const int window_height = calculate_window_height(state);

  return {total_width, window_height};
}

auto calculate_window_height(const Core::State::AppState& state) -> int {
  const auto counts = count_items_per_column(state.app_window->data.menu_items);
  const auto& render = state.app_window->layout;

  // 计算每列的高度
  const int ratio_height = counts.ratio_count * render.item_height;
  const int resolution_height = counts.resolution_count * render.item_height;
  const int settings_height = counts.settings_count * render.item_height;

  // 找出最大高度
  const int max_column_height = std::max({ratio_height, resolution_height, settings_height});

  // 返回总高度
  return render.title_height + render.separator_height + max_column_height;
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

auto get_item_index_from_point(const Core::State::AppState& state, int x, int y) -> int {
  const auto& render = state.app_window->layout;
  const auto& items = state.app_window->data.menu_items;

  // 检查是否在标题栏或分隔线区域
  if (y < render.title_height + render.separator_height) {
    return -1;
  }

  const auto bounds = get_column_bounds(state);

  // 确定点击的是哪一列
  UI::AppWindow::MenuItemCategory target_category;
  if (x < bounds.ratio_column_right) {
    target_category = UI::AppWindow::MenuItemCategory::AspectRatio;
  } else if (x < bounds.resolution_column_right) {
    target_category = UI::AppWindow::MenuItemCategory::Resolution;
  } else {
    // 设置列的特殊处理
    return get_settings_item_index(state, y);
  }

  // 处理比例和分辨率列
  int item_y = render.title_height + render.separator_height;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    if (item.category == target_category) {
      if (y >= item_y && y < item_y + render.item_height) {
        return static_cast<int>(i);
      }
      item_y += render.item_height;
    }
  }

  return -1;
}

auto count_items_per_column(const std::vector<UI::AppWindow::MenuItem>& items) -> ColumnCounts {
  ColumnCounts counts;

  for (const auto& item : items) {
    switch (item.category) {
      case UI::AppWindow::MenuItemCategory::AspectRatio:
        ++counts.ratio_count;
        break;
      case UI::AppWindow::MenuItemCategory::Resolution:
        ++counts.resolution_count;
        break;
      case UI::AppWindow::MenuItemCategory::Feature:
        ++counts.settings_count;
        break;
    }
  }

  return counts;
}

auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds {
  const auto& render = state.app_window->layout;
  const int ratio_column_right = render.ratio_column_width;
  const int resolution_column_right = ratio_column_right + render.resolution_column_width;
  const int settings_column_left = resolution_column_right + render.separator_height;

  return {ratio_column_right, resolution_column_right, settings_column_left};
}

auto get_settings_item_index(const Core::State::AppState& state, int y) -> int {
  const auto& render = state.app_window->layout;
  const auto& items = state.app_window->data.menu_items;

  int settings_y = render.title_height + render.separator_height;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];

    // 判断是否为功能项
    if (item.category == UI::AppWindow::MenuItemCategory::Feature) {
      if (y >= settings_y && y < settings_y + render.item_height) {
        return static_cast<int>(i);
      }
      settings_y += render.item_height;
    }
  }
  return -1;
}

auto get_indicator_width(const UI::AppWindow::MenuItem& item, const Core::State::AppState& state)
    -> int {
  return (item.category == UI::AppWindow::MenuItemCategory::AspectRatio)
             ? state.app_window->layout.ratio_indicator_width
             : state.app_window->layout.indicator_width;
}

}  // namespace UI::AppWindow