module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

module UI.AppWindow.Painter;

import std;
import Core.State;
import Types.UI;
import UI.AppWindow.Layout;
import UI.AppWindow.State;


namespace UI::AppWindow::Painter {

// 主绘制函数实现
auto paint_app_window(const Core::State::AppState& state, const RECT& client_rect) -> void {
  const auto& d2d = state.d2d_render;

  if (!d2d.is_initialized || !d2d.render_target) {
    return;
  }

  // 检查是否需要重新创建资源
  if (d2d.needs_resize) {
    // 这里应该触发resize操作，但我们暂时跳过
    return;
  }

  d2d.render_target->BeginDraw();

  const auto rect_f = Types::UI::rect_to_d2d(client_rect);

  // 绘制各个部分
  draw_app_background(state, rect_f);
  draw_app_title_bar(state, rect_f);
  draw_app_separators(state, rect_f);
  draw_app_items(state, rect_f);

  HRESULT hr = d2d.render_target->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    // 标记需要重新创建渲染目标
    const_cast<Core::State::AppState&>(state).d2d_render.needs_resize = true;
  }
}

// 绘制背景
auto draw_app_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.d2d_render;
  d2d.render_target->FillRectangle(rect, d2d.white_brush);
}

// 绘制标题栏
auto draw_app_title_bar(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.d2d_render;
  const auto& render = state.app_window.layout;

  // 绘制标题栏背景
  D2D1_RECT_F title_rect = Types::UI::make_d2d_rect(
      rect.left, rect.top, rect.right, rect.top + static_cast<float>(render.title_height));
  d2d.render_target->FillRectangle(title_rect, d2d.title_brush);

  // 绘制标题文本
  D2D1_RECT_F text_rect =
      Types::UI::make_d2d_rect(rect.left + static_cast<float>(render.text_padding), rect.top,
                               rect.right, rect.top + static_cast<float>(render.title_height));

  d2d.render_target->DrawText(L"SpinningMomo",
                              12,  // 文本长度
                              d2d.text_format, text_rect, d2d.text_brush);
}

// 绘制分隔线
auto draw_app_separators(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.d2d_render;
  const auto& render = state.app_window.layout;

  // 使用现有的辅助函数获取列边界
  const auto bounds = get_column_bounds(state);

  // 绘制水平分隔线
  D2D1_RECT_F h_sep_rect = Types::UI::make_d2d_rect(
      rect.left, rect.top + static_cast<float>(render.title_height), rect.right,
      rect.top + static_cast<float>(render.title_height + render.separator_height));
  d2d.render_target->FillRectangle(h_sep_rect, d2d.separator_brush);

  // 绘制垂直分隔线1
  D2D1_RECT_F v_sep_rect1 = Types::UI::make_d2d_rect(
      static_cast<float>(bounds.ratio_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.ratio_column_right + render.separator_height), rect.bottom);
  d2d.render_target->FillRectangle(v_sep_rect1, d2d.separator_brush);

  // 绘制垂直分隔线2
  D2D1_RECT_F v_sep_rect2 = Types::UI::make_d2d_rect(
      static_cast<float>(bounds.resolution_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.resolution_column_right + render.separator_height), rect.bottom);
  d2d.render_target->FillRectangle(v_sep_rect2, d2d.separator_brush);
}

// 绘制所有菜单项
auto draw_app_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render = state.app_window.layout;
  const auto& items = state.app_window.data.menu_items;
  const auto bounds = get_column_bounds(state);

  float y = rect.top + static_cast<float>(render.title_height + render.separator_height);
  float settings_y = y;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    D2D1_RECT_F item_rect{};

    // 根据项目类型确定绘制位置
    using ItemType = UI::AppWindow::ItemType;
    switch (item.type) {
      case ItemType::Ratio:
        item_rect =
            Types::UI::make_d2d_rect(rect.left, y, static_cast<float>(bounds.ratio_column_right),
                                     y + static_cast<float>(render.item_height));
        break;
      case ItemType::Resolution:
        item_rect = Types::UI::make_d2d_rect(
            static_cast<float>(bounds.ratio_column_right + render.separator_height), y,
            static_cast<float>(bounds.resolution_column_right),
            y + static_cast<float>(render.item_height));
        break;
      case ItemType::CaptureWindow:
      case ItemType::OpenScreenshot:
      case ItemType::PreviewWindow:
      case ItemType::OverlayWindow:
      case ItemType::LetterboxWindow:
      case ItemType::Reset:
      case ItemType::Hide:
      case ItemType::Exit:
        item_rect = Types::UI::make_d2d_rect(
            static_cast<float>(bounds.resolution_column_right + render.separator_height),
            settings_y, rect.right, settings_y + static_cast<float>(render.item_height));
        settings_y += static_cast<float>(render.item_height);
        break;
      default:
        continue;
    }

    const bool is_hovered = (static_cast<int>(i) == state.app_window.ui.hover_index);
    draw_app_single_item(state, item, item_rect, is_hovered);

    // 只有在同一列中才增加y坐标（复制现有逻辑）
    if ((i + 1 < items.size()) && (items[i + 1].type == item.type)) {
      if (item.type != ItemType::Reset) {
        y += static_cast<float>(render.item_height);
      }
    } else if (i + 1 < items.size() && items[i + 1].type != item.type) {
      if (items[i + 1].type != ItemType::Reset) {
        y = rect.top + static_cast<float>(render.title_height + render.separator_height);
      }
    }
  }
}

// 绘制单个菜单项
auto draw_app_single_item(const Core::State::AppState& state, const UI::AppWindow::MenuItem& item,
                          const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& d2d = state.d2d_render;
  const auto& render = state.app_window.layout;

  // 绘制悬停背景
  if (is_hovered) {
    d2d.render_target->FillRectangle(item_rect, d2d.hover_brush);
  }

  // 绘制选中指示器
  const bool is_selected = UI::AppWindow::is_item_selected(item, state.app_window.ui);
  if (is_selected) {
    const int indicator_width = get_indicator_width(item, state);
    D2D1_RECT_F indicator_rect = Types::UI::make_d2d_rect(
        item_rect.left, item_rect.top, item_rect.left + static_cast<float>(indicator_width),
        item_rect.bottom);
    d2d.render_target->FillRectangle(indicator_rect, d2d.indicator_brush);
  }

  // 绘制文本
  const int indicator_width = get_indicator_width(item, state);
  D2D1_RECT_F text_rect = Types::UI::make_d2d_rect(
      item_rect.left + static_cast<float>(render.text_padding + indicator_width), item_rect.top,
      item_rect.right, item_rect.bottom);

  d2d.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                              d2d.text_format, text_rect, d2d.text_brush);
}

}  // namespace UI::AppWindow::Painter