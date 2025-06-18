module;

#include <dwmapi.h>
#include <windows.h>

export module UI.AppWindow.Rendering;

import std;
import Core.State;
import Types.State;

namespace UI::AppWindow {

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

// 计算窗口尺寸
export auto calculate_window_size(const Core::State::AppState& state) -> SIZE;

// 计算窗口高度
export auto calculate_window_height(const Core::State::AppState& state) -> int;

// 计算窗口居中位置
export auto calculate_center_position(const SIZE& window_size) -> POINT;

// 从点击坐标获取菜单项索引
export auto get_item_index_from_point(const Core::State::AppState& state, int x, int y) -> int;

// 计算每列的项目数量
export auto count_items_per_column(const std::vector<Core::State::MenuItem>& items) -> ColumnCounts;

// 获取列边界
export auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds;

// 主绘制函数
export auto paint_window(HDC hdc, const RECT& client_rect, const Core::State::AppState& state)
    -> void;

// 创建DPI缩放的字体
export auto create_scaled_font(int font_size) -> HFONT;

// 获取设置列中的项目索引
auto get_settings_item_index(const Core::State::AppState& state, int y) -> int;

// 绘制背景
auto draw_background(HDC mem_dc, const RECT& rect) -> void;

// 绘制标题栏
auto draw_title_bar(HDC mem_dc, const RECT& rect, const Core::State::AppState& state) -> void;

// 绘制分隔线
auto draw_separators(HDC mem_dc, const RECT& rect, const Core::State::AppState& state) -> void;

// 绘制所有菜单项
auto draw_items(HDC mem_dc, const RECT& rect, const Core::State::AppState& state, HFONT font)
    -> void;

// 绘制单个菜单项
auto draw_single_item(HDC mem_dc, const Core::State::MenuItem& item, const RECT& item_rect,
                      const Core::State::AppState& state, bool is_hovered, HFONT font) -> void;

// 获取指示器宽度
auto get_indicator_width(const Core::State::MenuItem& item, const Core::State::AppState& state)
    -> int;

}  // namespace UI::AppWindow