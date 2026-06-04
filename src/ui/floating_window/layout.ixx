module;

export module UI.FloatingWindow.Layout;

import std;
import Core.State;
import UI.FloatingWindow.Types;
import <windows.h>;

namespace UI::FloatingWindow::Layout {

// 列计数结构
export struct ColumnCounts {
  int ratio_count = 0;
  int resolution_count = 0;
  int feature_count = 0;
};

// 列边界结构
export struct ColumnBounds {
  int ratio_column_right;
  int resolution_column_right;
  int feature_column_left;
};

// 基于指定 DPI 计算布局与窗口尺寸
export auto calculate_window_metrics(const Core::State::AppState& state, UINT dpi)
    -> UI::FloatingWindow::WindowMetrics;

// 计算窗口居中位置
export auto calculate_center_position(const SIZE& window_size) -> POINT;

// 从点击坐标获取菜单项索引
export auto get_item_index_from_point(const Core::State::AppState& state, int x, int y) -> int;

// 计算每列的项目数量
export auto count_items_per_column(const std::vector<UI::FloatingWindow::MenuItem>& items)
    -> ColumnCounts;

// 获取列边界
export auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds;

// 获取指示器宽度
export auto get_indicator_width(const UI::FloatingWindow::MenuItem& item,
                                const Core::State::AppState& state) -> int;

// 获取功能列中的项目索引
auto get_feature_item_index(const Core::State::AppState& state, int y) -> int;

}  // namespace UI::FloatingWindow::Layout
