module;

#include <dwmapi.h>
#include <windows.h>

module UI.AppWindow.Rendering;

import std;
import Core.State;

namespace UI::AppWindow {

auto calculate_window_size(const Core::State::AppState& state) -> SIZE {
  const auto& render = state.render;
  const int total_width =
      render.ratio_column_width + render.resolution_column_width + render.settings_column_width;
  const int window_height = calculate_window_height(state);

  return {total_width, window_height};
}

auto calculate_window_height(const Core::State::AppState& state) -> int {
  const auto counts = count_items_per_column(state.data.menu_items);
  const auto& render = state.render;

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
  const auto& render = state.render;
  const auto& items = state.data.menu_items;

  // 检查是否在标题栏或分隔线区域
  if (y < render.title_height + render.separator_height) {
    return -1;
  }

  const auto bounds = get_column_bounds(state);

  // 确定点击的是哪一列
  Core::State::ItemType target_type;
  if (x < bounds.ratio_column_right) {
    target_type = Core::State::ItemType::Ratio;
  } else if (x < bounds.resolution_column_right) {
    target_type = Core::State::ItemType::Resolution;
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

auto count_items_per_column(const std::vector<Core::State::MenuItem>& items) -> ColumnCounts {
  ColumnCounts counts;

  for (const auto& item : items) {
    using ItemType = Core::State::ItemType;
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
      case ItemType::Close:
      case ItemType::Exit:
        ++counts.settings_count;
        break;
    }
  }

  return counts;
}

auto get_column_bounds(const Core::State::AppState& state) -> ColumnBounds {
  const auto& render = state.render;
  const int ratio_column_right = render.ratio_column_width;
  const int resolution_column_right = ratio_column_right + render.resolution_column_width;
  const int settings_column_left = resolution_column_right + render.separator_height;

  return {ratio_column_right, resolution_column_right, settings_column_left};
}

auto paint_window(HDC hdc, const RECT& client_rect, const Core::State::AppState& state) -> void {
  // 创建双缓冲
  HDC mem_dc = CreateCompatibleDC(hdc);
  HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, client_rect.right, client_rect.bottom);
  HBITMAP old_bitmap = static_cast<HBITMAP>(SelectObject(mem_dc, mem_bitmap));

  // 设置文本属性，使用 DPI 感知的字体大小
  SetBkMode(mem_dc, TRANSPARENT);
  HFONT h_font = create_scaled_font(state.render.font_size);
  HFONT old_font = static_cast<HFONT>(SelectObject(mem_dc, h_font));

  // 绘制各个部分
  draw_background(mem_dc, client_rect);
  draw_title_bar(mem_dc, client_rect, state);
  draw_separators(mem_dc, client_rect, state);
  draw_items(mem_dc, client_rect, state, h_font);

  // 复制到屏幕
  BitBlt(hdc, 0, 0, client_rect.right, client_rect.bottom, mem_dc, 0, 0, SRCCOPY);

  // 清理资源
  SelectObject(mem_dc, old_font);
  DeleteObject(h_font);
  SelectObject(mem_dc, old_bitmap);
  DeleteObject(mem_bitmap);
  DeleteDC(mem_dc);
}

auto create_scaled_font(int font_size) -> HFONT {
  return CreateFontW(-font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
}

auto get_settings_item_index(const Core::State::AppState& state, int y) -> int {
  const auto& render = state.render;
  const auto& items = state.data.menu_items;

  int settings_y = render.title_height + render.separator_height;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    using ItemType = Core::State::ItemType;
    if (item.type == ItemType::CaptureWindow || item.type == ItemType::OpenScreenshot ||
        item.type == ItemType::PreviewWindow || item.type == ItemType::OverlayWindow ||
        item.type == ItemType::LetterboxWindow || item.type == ItemType::Reset ||
        item.type == ItemType::Close || item.type == ItemType::Exit) {
      if (y >= settings_y && y < settings_y + render.item_height) {
        return static_cast<int>(i);
      }
      settings_y += render.item_height;
    }
  }
  return -1;
}

auto draw_background(HDC mem_dc, const RECT& rect) -> void {
  HBRUSH h_back_brush = CreateSolidBrush(RGB(255, 255, 255));
  FillRect(mem_dc, &rect, h_back_brush);
  DeleteObject(h_back_brush);
}

auto draw_title_bar(HDC mem_dc, const RECT& rect, const Core::State::AppState& state) -> void {
  // 绘制标题栏
  RECT title_rect = rect;
  title_rect.bottom = state.render.title_height;
  HBRUSH h_title_brush = CreateSolidBrush(RGB(240, 240, 240));
  FillRect(mem_dc, &title_rect, h_title_brush);
  DeleteObject(h_title_brush);

  // 绘制标题文本
  SetTextColor(mem_dc, RGB(51, 51, 51));
  title_rect.left += state.render.text_padding;
  DrawTextW(mem_dc, L"SpinningMomo", -1, &title_rect,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);
}

auto draw_separators(HDC mem_dc, const RECT& rect, const Core::State::AppState& state) -> void {
  const auto bounds = get_column_bounds(state);
  const auto& render = state.render;

  // 绘制水平分隔线
  RECT sep_rect{rect.left, render.title_height, rect.right,
                render.title_height + render.separator_height};
  HBRUSH h_sep_brush = CreateSolidBrush(RGB(229, 229, 229));
  FillRect(mem_dc, &sep_rect, h_sep_brush);

  // 绘制垂直分隔线
  RECT v_sep_rect1{bounds.ratio_column_right, render.title_height,
                   bounds.ratio_column_right + render.separator_height, rect.bottom};
  RECT v_sep_rect2{bounds.resolution_column_right, render.title_height,
                   bounds.resolution_column_right + render.separator_height, rect.bottom};
  FillRect(mem_dc, &v_sep_rect1, h_sep_brush);
  FillRect(mem_dc, &v_sep_rect2, h_sep_brush);
  DeleteObject(h_sep_brush);
}

auto draw_items(HDC mem_dc, const RECT& rect, const Core::State::AppState& state, HFONT font)
    -> void {
  const auto bounds = get_column_bounds(state);
  const auto& render = state.render;
  const auto& items = state.data.menu_items;

  int y = render.title_height + render.separator_height;
  int settings_y = y;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    RECT item_rect{};

    // 根据项目类型确定绘制位置
    using ItemType = Core::State::ItemType;
    switch (item.type) {
      case ItemType::Ratio:
        item_rect = {0, y, bounds.ratio_column_right, y + render.item_height};
        break;
      case ItemType::Resolution:
        item_rect = {bounds.ratio_column_right + render.separator_height, y,
                     bounds.resolution_column_right, y + render.item_height};
        break;
      case ItemType::CaptureWindow:
      case ItemType::OpenScreenshot:
      case ItemType::PreviewWindow:
      case ItemType::OverlayWindow:
      case ItemType::LetterboxWindow:
      case ItemType::Reset:
      case ItemType::Close:
      case ItemType::Exit:
        item_rect = {bounds.resolution_column_right + render.separator_height, settings_y,
                     rect.right, settings_y + render.item_height};
        settings_y += render.item_height;
        break;
      default:
        continue;
    }

    const bool is_hovered = (static_cast<int>(i) == state.ui.hover_index);
    draw_single_item(mem_dc, item, item_rect, state, is_hovered, font);

    // 只有在同一列中才增加y坐标
    if ((i + 1 < items.size()) && (items[i + 1].type == item.type)) {
      if (item.type != ItemType::Reset) {
        y += render.item_height;
      }
    } else if (i + 1 < items.size() && items[i + 1].type != item.type) {
      if (items[i + 1].type != ItemType::Reset) {
        y = render.title_height + render.separator_height;
      }
    }
  }
}

auto draw_single_item(HDC mem_dc, const Core::State::MenuItem& item, const RECT& item_rect,
                      const Core::State::AppState& state, bool is_hovered, HFONT font) -> void {
  // 绘制悬停背景
  if (is_hovered) {
    HBRUSH h_hover_brush = CreateSolidBrush(RGB(242, 242, 242));
    FillRect(mem_dc, &item_rect, h_hover_brush);
    DeleteObject(h_hover_brush);
  }

  // 绘制选中指示器
  const bool is_selected = Core::State::is_item_selected(item, state.ui);
  if (is_selected) {
    const int indicator_width = get_indicator_width(item, state);
    RECT indicator_rect{item_rect.left, item_rect.top, item_rect.left + indicator_width,
                        item_rect.bottom};
    HBRUSH h_indicator_brush = CreateSolidBrush(RGB(255, 160, 80));
    FillRect(mem_dc, &indicator_rect, h_indicator_brush);
    DeleteObject(h_indicator_brush);
  }

  // 绘制文本
  RECT text_rect = item_rect;
  text_rect.left += state.render.text_padding + get_indicator_width(item, state);
  SetTextColor(mem_dc, RGB(51, 51, 51));
  DrawTextW(mem_dc, item.text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

auto get_indicator_width(const Core::State::MenuItem& item, const Core::State::AppState& state)
    -> int {
  return (item.type == Core::State::ItemType::Ratio) ? state.render.ratio_indicator_width
                                                     : state.render.indicator_width;
}

}  // namespace UI::AppWindow