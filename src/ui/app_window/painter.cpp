module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

module UI.AppWindow.Painter;

import std;
import Core.State;
import UI.AppWindow.Layout;
import UI.AppWindow.State;
import UI.AppWindow.Types;
import UI.AppWindow.D2DContext;
import Common.MenuIds;

namespace UI::AppWindow::Painter {

// 主绘制函数实现
auto paint_app_window(Core::State::AppState& state, HWND hwnd, const RECT& client_rect) -> void {
  auto& d2d = state.app_window->d2d_context;

  if (!d2d.is_initialized || !d2d.render_target) {
    return;
  }

  // 1. 先处理渲染目标resize（如果需要）
  if (d2d.needs_resize) {
    if (!UI::AppWindow::D2DContext::resize_d2d(
            state, {client_rect.right - client_rect.left, client_rect.bottom - client_rect.top})) {
      return;  // resize失败，无法继续绘制
    }
  }

  // 2. 再处理字体更新（如果需要）
  if (d2d.needs_font_update) {
    if (!UI::AppWindow::D2DContext::update_text_format_if_needed(state)) {
      return;  // 字体更新失败，无法继续绘制
    }
  }

  if (d2d.is_rendering) {
    return;
  }

  d2d.is_rendering = true;

  d2d.render_target->BeginDraw();

  // 清空背景为完全透明
  d2d.render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  // 全局设置替换混合模式，避免所有颜色叠加
  if (d2d.device_context) {
    d2d.device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
  }

  const auto rect_f = UI::AppWindow::rect_to_d2d(client_rect);

  // 4. 绘制各个部分
  draw_app_background(state, rect_f);
  draw_app_title_bar(state, rect_f);
  draw_app_separators(state, rect_f);
  draw_app_items(state, rect_f);

  HRESULT hr = d2d.render_target->EndDraw();

  // 处理设备丢失等错误
  if (hr == D2DERR_RECREATE_TARGET) {
    // 设备丢失，标记需要重新创建渲染目标
    d2d.needs_resize = true;
  }

  d2d.is_rendering = false;

  // 5. 更新分层窗口
  if (SUCCEEDED(hr)) {
    update_layered_window(state, hwnd);
  }
}

// 绘制背景
auto draw_app_background(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.app_window->d2d_context;
  // 使用半透明白色背景
  d2d.render_target->FillRectangle(rect, d2d.background_brush);
}

// 绘制关闭按钮
auto draw_close_button(const Core::State::AppState& state, const D2D1_RECT_F& title_rect) -> void {
  const auto& d2d = state.app_window->d2d_context;
  const auto& render = state.app_window->layout;

  // 计算按钮尺寸（正方形，与标题栏高度一致）
  const float button_size = static_cast<float>(render.title_height);
  const float button_padding = static_cast<float>(render.text_padding) / 2.0f;

  // 计算按钮位置（右上角）
  const float x = title_rect.right - button_size;
  const float y = title_rect.top;

  // 创建按钮区域矩形
  const D2D1_RECT_F button_rect = D2D1::RectF(x, y, x + button_size, y + button_size);

  // 绘制悬停背景（如果需要）
  if (state.app_window->ui.close_button_hovered) {
    d2d.render_target->FillRectangle(button_rect, d2d.hover_brush);
  }

  // 计算"X"图标尺寸和位置
  const float icon_margin = button_size * 0.35f;  // 边距
  const float icon_size = button_size - 2 * icon_margin;

  const float icon_left = x + icon_margin;
  const float icon_top = y + icon_margin;
  const float icon_right = icon_left + icon_size;
  const float icon_bottom = icon_top + icon_size;

  // 绘制"X"图标
  const float pen_width = 1.5f;
  d2d.render_target->DrawLine(D2D1::Point2F(icon_left, icon_top),
                              D2D1::Point2F(icon_right, icon_bottom), d2d.text_brush, pen_width,
                              nullptr);

  d2d.render_target->DrawLine(D2D1::Point2F(icon_right, icon_top),
                              D2D1::Point2F(icon_left, icon_bottom), d2d.text_brush, pen_width,
                              nullptr);
}

// 绘制标题栏
auto draw_app_title_bar(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.app_window->d2d_context;
  const auto& render = state.app_window->layout;

  // 绘制标题栏背景
  D2D1_RECT_F title_rect = UI::AppWindow::make_d2d_rect(
      rect.left, rect.top, rect.right, rect.top + static_cast<float>(render.title_height));
  d2d.render_target->FillRectangle(title_rect, d2d.title_brush);

  // 绘制标题文本（保持完全不透明）
  D2D1_RECT_F text_rect =
      UI::AppWindow::make_d2d_rect(rect.left + static_cast<float>(render.text_padding), rect.top,
                                   rect.right, rect.top + static_cast<float>(render.title_height));

  d2d.render_target->DrawText(L"SpinningMomo",
                              12,  // 文本长度
                              d2d.text_format, text_rect, d2d.text_brush);

  // 绘制关闭按钮
  draw_close_button(state, title_rect);
}

// 绘制分隔线
auto draw_app_separators(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& d2d = state.app_window->d2d_context;
  const auto& render = state.app_window->layout;

  // 使用简单的列边界计算
  const auto bounds = UI::AppWindow::Layout::get_column_bounds(state);

  // 绘制水平分隔线（使用半透明画刷）
  D2D1_RECT_F h_sep_rect = UI::AppWindow::make_d2d_rect(
      rect.left, rect.top + static_cast<float>(render.title_height), rect.right,
      rect.top + static_cast<float>(render.title_height + render.separator_height));
  d2d.render_target->FillRectangle(h_sep_rect, d2d.separator_brush);

  // 绘制垂直分隔线1（使用半透明画刷）
  D2D1_RECT_F v_sep_rect1 = UI::AppWindow::make_d2d_rect(
      static_cast<float>(bounds.ratio_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.ratio_column_right + render.separator_height), rect.bottom);
  d2d.render_target->FillRectangle(v_sep_rect1, d2d.separator_brush);

  // 绘制垂直分隔线2（使用半透明画刷）
  D2D1_RECT_F v_sep_rect2 = UI::AppWindow::make_d2d_rect(
      static_cast<float>(bounds.resolution_column_right),
      rect.top + static_cast<float>(render.title_height),
      static_cast<float>(bounds.resolution_column_right + render.separator_height), rect.bottom);
  d2d.render_target->FillRectangle(v_sep_rect2, d2d.separator_brush);
}

// 绘制所有菜单项
auto draw_app_items(const Core::State::AppState& state, const D2D1_RECT_F& rect) -> void {
  const auto& render = state.app_window->layout;
  const auto& items = state.app_window->data.menu_items;
  const auto bounds = UI::AppWindow::Layout::get_column_bounds(state);

  float y = rect.top + static_cast<float>(render.title_height + render.separator_height);
  float settings_y = y;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    D2D1_RECT_F item_rect{};

    // 根据项目类别确定绘制位置
    switch (item.category) {
      case UI::AppWindow::MenuItemCategory::AspectRatio:
        item_rect = UI::AppWindow::make_d2d_rect(rect.left, y,
                                                 static_cast<float>(bounds.ratio_column_right),
                                                 y + static_cast<float>(render.item_height));
        break;
      case UI::AppWindow::MenuItemCategory::Resolution:
        item_rect = UI::AppWindow::make_d2d_rect(
            static_cast<float>(bounds.ratio_column_right + render.separator_height), y,
            static_cast<float>(bounds.resolution_column_right),
            y + static_cast<float>(render.item_height));
        break;
      case UI::AppWindow::MenuItemCategory::Feature:
        item_rect = UI::AppWindow::make_d2d_rect(
            static_cast<float>(bounds.resolution_column_right + render.separator_height),
            settings_y, rect.right, settings_y + static_cast<float>(render.item_height));
        settings_y += static_cast<float>(render.item_height);
        break;
      default:
        continue;
    }

    const bool is_hovered = (static_cast<int>(i) == state.app_window->ui.hover_index);
    draw_app_single_item(state, item, item_rect, is_hovered);

    // 只有在同一列中才增加y坐标（复制现有逻辑）
    if ((i + 1 < items.size()) && (items[i + 1].category == item.category)) {
      if (item.category != UI::AppWindow::MenuItemCategory::Feature ||
          item.action_id !=
              Common::MenuIds::to_string(Common::MenuIds::Id::WindowControlResetTransform)) {
        y += static_cast<float>(render.item_height);
      }
    } else if (i + 1 < items.size() && items[i + 1].category != item.category) {
      if (items[i + 1].category != UI::AppWindow::MenuItemCategory::Feature ||
          items[i + 1].action_id !=
              Common::MenuIds::to_string(Common::MenuIds::Id::WindowControlResetTransform)) {
        y = rect.top + static_cast<float>(render.title_height + render.separator_height);
      }
    }
  }
}

// 绘制单个菜单项
auto draw_app_single_item(const Core::State::AppState& state, const UI::AppWindow::MenuItem& item,
                          const D2D1_RECT_F& item_rect, bool is_hovered) -> void {
  const auto& d2d = state.app_window->d2d_context;
  const auto& render = state.app_window->layout;

  // 绘制悬停背景
  if (is_hovered) {
    d2d.render_target->FillRectangle(item_rect, d2d.hover_brush);
  }

  // 绘制选中指示器（保持完全不透明）
  const bool is_selected = UI::AppWindow::State::is_item_selected(item, state.app_window->ui);
  if (is_selected) {
    const int indicator_width = UI::AppWindow::Layout::get_indicator_width(item, state);
    D2D1_RECT_F indicator_rect = UI::AppWindow::make_d2d_rect(
        item_rect.left, item_rect.top, item_rect.left + static_cast<float>(indicator_width),
        item_rect.bottom);
    d2d.render_target->FillRectangle(indicator_rect, d2d.indicator_brush);
  }

  // 绘制文本（保持完全不透明）
  const int indicator_width = UI::AppWindow::Layout::get_indicator_width(item, state);

  D2D1_RECT_F text_rect = UI::AppWindow::make_d2d_rect(
      item_rect.left + static_cast<float>(render.text_padding + indicator_width), item_rect.top,
      item_rect.right - static_cast<float>(render.text_padding / 2), item_rect.bottom);

  // 计算可用于文本的宽度
  const float available_width = text_rect.right - text_rect.left;

  // 如果文本为空或宽度无效，则直接使用默认字体绘制
  if (item.text.empty() || available_width <= 0.0f) {
    d2d.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                d2d.text_format, text_rect, d2d.text_brush);
    return;
  }

  // 测量当前字体大小下的文本宽度
  float text_width =
      UI::AppWindow::D2DContext::measure_text_width(item.text, d2d.text_format, d2d.write_factory);

  // 如果文本宽度小于可用宽度，则直接使用默认字体绘制
  if (text_width <= available_width) {
    d2d.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                d2d.text_format, text_rect, d2d.text_brush);
    return;
  }

  // 文本太宽，需要调整字体大小
  IDWriteTextFormat* adjusted_text_format = nullptr;
  float adjusted_font_size = render.font_size;

  // 逐步减小字体大小，直到文本适合或达到最小字体大小
  while (adjusted_font_size > UI::AppWindow::LayoutConfig::MIN_FONT_SIZE) {
    adjusted_font_size -= UI::AppWindow::LayoutConfig::FONT_SIZE_STEP;

    // 如果已达到最小字体大小，则使用最小字体大小
    if (adjusted_font_size <= UI::AppWindow::LayoutConfig::MIN_FONT_SIZE) {
      adjusted_font_size = UI::AppWindow::LayoutConfig::MIN_FONT_SIZE;
    }

    // 创建调整后的文本格式
    adjusted_text_format = UI::AppWindow::D2DContext::create_text_format_with_size(
        d2d.write_factory, adjusted_font_size);
    if (!adjusted_text_format) {
      // 如果创建失败，则回退到默认字体
      break;
    }

    // 测量调整后字体大小的文本宽度
    text_width = UI::AppWindow::D2DContext::measure_text_width(item.text, adjusted_text_format,
                                                               d2d.write_factory);

    // 如果文本宽度满足要求，则使用调整后的字体绘制
    if (text_width <= available_width) {
      d2d.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                                  adjusted_text_format, text_rect, d2d.text_brush);
      adjusted_text_format->Release();
      return;
    }

    // 释放临时文本格式对象
    adjusted_text_format->Release();
    adjusted_text_format = nullptr;

    // 如果已达到最小字体大小，则跳出循环
    if (adjusted_font_size <= UI::AppWindow::LayoutConfig::MIN_FONT_SIZE) {
      break;
    }
  }

  // 如果无法调整到合适的字体大小，仍然使用默认字体绘制（可能会被截断）
  d2d.render_target->DrawText(item.text.c_str(), static_cast<UINT32>(item.text.length()),
                              d2d.text_format, text_rect, d2d.text_brush);
}

// UpdateLayeredWindow函数 - 将内存DC更新到分层窗口
auto update_layered_window(const Core::State::AppState& state, HWND hwnd) -> void {
  const auto& d2d = state.app_window->d2d_context;

  if (!d2d.memory_dc || !d2d.is_initialized) {
    return;
  }

  // 配置Alpha混合
  BLENDFUNCTION blend_func = {};
  blend_func.BlendOp = AC_SRC_OVER;
  blend_func.BlendFlags = 0;
  blend_func.SourceConstantAlpha = 255;
  blend_func.AlphaFormat = AC_SRC_ALPHA;

  // 源点和窗口大小
  POINT src_point = {0, 0};
  SIZE window_size = d2d.bitmap_size;

  // 更新分层窗口
  UpdateLayeredWindow(hwnd,           // 目标窗口
                      nullptr,        // 桌面DC（使用默认）
                      nullptr,        // 窗口位置（不改变）
                      &window_size,   // 窗口大小
                      d2d.memory_dc,  // 源DC
                      &src_point,     // 源起始点
                      0,              // 颜色键（不使用）
                      &blend_func,    // Alpha混合函数
                      ULW_ALPHA       // 使用Alpha通道
  );
}

}  // namespace UI::AppWindow::Painter