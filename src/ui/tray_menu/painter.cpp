module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

#include <iostream>

module UI.TrayMenu.Painter;

import std;
import Core.State;
import Types.UI;
import UI.TrayMenu.State;

namespace UI::TrayMenu::Painter {

// 主绘制函数实现
auto paint_tray_menu(const Core::State::AppState& state, const RECT& client_rect) -> void {
  const auto& d2d = state.d2d_render;
  const auto& tray_menu = state.tray_menu;

  // 检查托盘菜单的渲染目标是否已初始化
  if (!tray_menu.render_target_initialized || !tray_menu.render_target) {
    return;
  }

  // 检查全局D2D资源是否可用（字体等）
  if (!d2d.is_initialized) {
    return;
  }

  // 检查托盘菜单的画刷是否已初始化
  if (!tray_menu.brushes_initialized) {
    return;
  }

  tray_menu.render_target->BeginDraw();

  const auto rect_f = Types::UI::rect_to_d2d(client_rect);

  // 绘制各个部分
  draw_menu_background(state, rect_f);
  draw_menu_items(state, rect_f);

  HRESULT hr = tray_menu.render_target->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    // 标记托盘菜单的渲染目标需要重新创建
    const_cast<Core::State::AppState&>(state).tray_menu.render_target_initialized = false;
  } else if (FAILED(hr)) {
    // 记录其他绘制错误
    // 这里可以添加日志记录，但要避免在绘制循环中频繁记录
  }
}

// 绘制菜单背景
auto draw_menu_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& tray_menu = state.tray_menu;

  // 清除背景
  tray_menu.render_target->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.95f));

  // 绘制圆角矩形背景
  const auto& layout = tray_menu.layout;
  D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(rect, static_cast<float>(layout.border_radius),
                                                     static_cast<float>(layout.border_radius));

  tray_menu.render_target->FillRoundedRectangle(rounded_rect, tray_menu.white_brush);

  // 绘制边框（可选）
  // tray_menu.render_target->DrawRoundedRectangle(rounded_rect, tray_menu.separator_brush, 1.0f);
}

// 绘制所有菜单项
auto draw_menu_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  float current_y = rect.top + static_cast<float>(layout.padding);

  for (size_t i = 0; i < tray_menu.items.size(); ++i) {
    const auto& item = tray_menu.items[i];
    bool is_hovered = (static_cast<int>(i) == tray_menu.interaction.hover_index);

    if (item.type == UI::TrayMenu::State::MenuItemType::Separator) {
      // 绘制分隔线
      float separator_height = static_cast<float>(layout.separator_height);
      D2D1_RECT_F separator_rect = D2D1::RectF(
          rect.left + static_cast<float>(layout.text_padding), current_y,
          rect.right - static_cast<float>(layout.text_padding), current_y + separator_height);
      draw_separator(state, separator_rect);
      current_y += separator_height;
    } else {
      // 绘制普通菜单项
      float item_height = static_cast<float>(layout.item_height);
      D2D1_RECT_F item_rect =
          D2D1::RectF(rect.left, current_y, rect.right, current_y + item_height);
      draw_single_menu_item(state, item, item_rect, is_hovered);
      current_y += item_height;
    }
  }
}

// 绘制单个菜单项
auto draw_single_menu_item(const Core::State::AppState& state,
                           const UI::TrayMenu::State::MenuItem& item, const D2D1_RECT_F& item_rect,
                           bool is_hovered) -> void {
  const auto& d2d = state.d2d_render;
  const auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  // 绘制悬停背景
  if (is_hovered && item.is_enabled) {
    tray_menu.render_target->FillRectangle(item_rect, tray_menu.hover_brush);
  }

  // 绘制文本
  D2D1_RECT_F text_rect =
      D2D1::RectF(item_rect.left + static_cast<float>(layout.text_padding), item_rect.top,
                  item_rect.right - static_cast<float>(layout.text_padding), item_rect.bottom);

  // 选择文本颜色
  ID2D1SolidColorBrush* text_brush =
      item.is_enabled ? tray_menu.text_brush : tray_menu.separator_brush;

  tray_menu.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                    d2d.text_format, text_rect, text_brush);

  // 绘制选中标记（如果有）
  if (item.is_checked) {
    // 在右侧绘制一个小的选中标记
    float check_size = static_cast<float>(layout.font_size) * 0.8f;
    float check_x = item_rect.right - static_cast<float>(layout.text_padding) - check_size;
    float check_y = item_rect.top + (item_rect.bottom - item_rect.top - check_size) / 2;

    D2D1_RECT_F check_rect =
        D2D1::RectF(check_x, check_y, check_x + check_size, check_y + check_size);

    tray_menu.render_target->FillRectangle(check_rect, tray_menu.indicator_brush);
  }
}

// 绘制分隔线
auto draw_separator(const Core::State::AppState& state, const D2D1_RECT_F& separator_rect) -> void {
  const auto& tray_menu = state.tray_menu;
  tray_menu.render_target->FillRectangle(separator_rect, tray_menu.separator_brush);
}

}  // namespace UI::TrayMenu::Painter
