module;

#include <dwmapi.h>
#include <windows.h>

module UI.AppWindow.Layout;

import std;
import Core.State;
import Types.UI;
import Utils.Logger;

namespace UI::AppWindow {

auto calculate_window_size(const Core::State::AppState& state) -> SIZE {
  const auto& render = state.app_window.layout;
  const int total_width =
      render.ratio_column_width + render.resolution_column_width + render.settings_column_width;
  const int window_height = calculate_window_height(state);

  return {total_width, window_height};
}

auto calculate_window_height(const Core::State::AppState& state) -> int {
  const auto counts = count_items_per_column(state.app_window.data.menu_items);
  const auto& render = state.app_window.layout;

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
  const auto& render = state.app_window.layout;
  const auto& items = state.app_window.data.menu_items;

  // 检查是否在标题栏或分隔线区域
  if (y < render.title_height + render.separator_height) {
    return -1;
  }

  const auto bounds = get_column_bounds(state);

  // 确定点击的是哪一列
  UI::AppWindow::ItemType target_type;
  if (x < bounds.ratio_column_right) {
    target_type = UI::AppWindow::ItemType::Ratio;
  } else if (x < bounds.resolution_column_right) {
    target_type = UI::AppWindow::ItemType::Resolution;
  } else {
    // 设置列的特殊处理
    return get_settings_item_index(state, y);
  }

  // 处理比例和分辨率列
  int item_y = render.title_height + render.separator_height;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    if (item.type == target_type) {
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
    using ItemType = UI::AppWindow::ItemType;
    switch (item.type) {
      case ItemType::Ratio:
        ++counts.ratio_count;
        break;
      case ItemType::Resolution:
        ++counts.resolution_count;
        break;
      case ItemType::CaptureWindow:
      case ItemType::OpenScreenshot:
      case ItemType::OverlayWindow:
      case ItemType::LetterboxWindow:
      case ItemType::PreviewWindow:
      case ItemType::Reset:
      case ItemType::Hide:
      case ItemType::Exit:
        ++counts.settings_count;
        break;
    }
  }

  return counts;
}

auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds {
  const auto& render = state.app_window.layout;
  const int ratio_column_right = render.ratio_column_width;
  const int resolution_column_right = ratio_column_right + render.resolution_column_width;
  const int settings_column_left = resolution_column_right + render.separator_height;

  return {ratio_column_right, resolution_column_right, settings_column_left};
}

auto get_settings_item_index(const Core::State::AppState& state, int y) -> int {
  const auto& render = state.app_window.layout;
  const auto& items = state.app_window.data.menu_items;

  int settings_y = render.title_height + render.separator_height;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    using ItemType = UI::AppWindow::ItemType;
    if (item.type == ItemType::CaptureWindow || item.type == ItemType::OpenScreenshot ||
        item.type == ItemType::PreviewWindow || item.type == ItemType::OverlayWindow ||
        item.type == ItemType::LetterboxWindow || item.type == ItemType::Reset ||
        item.type == ItemType::Hide || item.type == ItemType::Exit) {
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
  return (item.type == UI::AppWindow::ItemType::Ratio)
             ? state.app_window.layout.ratio_indicator_width
             : state.app_window.layout.indicator_width;
}

}  // namespace UI::AppWindow