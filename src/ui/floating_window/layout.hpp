#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/types.hpp"

namespace ui::floating_window::layout {

// 列计数结构
struct ColumnCounts {
  int ratio_count = 0;
  int resolution_count = 0;
  int feature_count = 0;
};

// 列边界结构
struct ColumnBounds {
  int ratio_column_right;
  int resolution_column_right;
  int feature_column_left;
};

// 基于指定 DPI 计算布局与窗口尺寸
auto calculate_window_metrics(const core::AppState& state, UINT dpi)
    -> ui::floating_window::WindowMetrics;

// 计算窗口居中位置
auto calculate_center_position(const SIZE& window_size) -> POINT;

// 从点击坐标获取菜单项索引
auto get_item_index_from_point(const core::AppState& state, int x, int y) -> int;

// 计算每列的项目数量
auto count_items_per_column(const std::vector<ui::floating_window::MenuItem>& items)
    -> ColumnCounts;

// 获取列边界
auto get_column_bounds(const core::AppState& state) -> ColumnBounds;

// 获取功能列中的项目索引
auto get_feature_item_index(const core::AppState& state, int y) -> int;

}  // namespace ui::floating_window::layout
