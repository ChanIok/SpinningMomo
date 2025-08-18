module;

#include <dwmapi.h>
#include <windows.h>

export module UI.AppWindow.Layout;

import std;
import Core.State;
import UI.AppWindow.State;
import UI.AppWindow.Types;

namespace UI::AppWindow::Layout {

// 列计数结构
export struct ColumnCounts {
  int ratio_count = 0;
  int resolution_count = 0;
  int settings_count = 0;
};

// 列边界结构
export struct ColumnBounds {
  int ratio_column_right;
  int resolution_column_right;
  int settings_column_left;
};

// 更新布局配置
export auto update_layout(Core::State::AppState& state) -> void;

// 计算窗口尺寸
export auto calculate_window_size(const Core::State::AppState& state) -> SIZE;

// 计算窗口高度
export auto calculate_window_height(const Core::State::AppState& state) -> int;

// 计算窗口居中位置
export auto calculate_center_position(const SIZE& window_size) -> POINT;

// 从点击坐标获取菜单项索引
export auto get_item_index_from_point(const Core::State::AppState& state, int x, int y) -> int;

// 计算每列的项目数量
export auto count_items_per_column(const std::vector<UI::AppWindow::MenuItem>& items)
    -> ColumnCounts;

// 获取列边界
export auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds;

// 获取指示器宽度
export auto get_indicator_width(const UI::AppWindow::MenuItem& item,
                                const Core::State::AppState& state) -> int;

// 获取设置列中的项目索引
auto get_settings_item_index(const Core::State::AppState& state, int y) -> int;

}  // namespace UI::AppWindow