module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <iostream>

module UI.TrayMenu.Layout;

import std;
import Core.State;
import UI.TrayMenu.State;
import Vendor.Windows;

namespace UI::TrayMenu::Layout {

// 计算文本宽度
auto calculate_text_width(const Core::State::AppState& state, const std::wstring& text) -> int {
  const auto& d2d = state.d2d_render;

  if (!d2d.is_initialized || !d2d.text_format) {
    // 如果D2D未初始化，使用估算值
    return static_cast<int>(text.length() * state.tray_menu.layout.font_size * 0.6);
  }

  // 使用DirectWrite测量文本
  Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
  HRESULT hr = d2d.write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), d2d.text_format,
      1000.0f,  // 最大宽度
      static_cast<float>(state.tray_menu.layout.item_height), text_layout.GetAddressOf());

  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS metrics;
    hr = text_layout->GetMetrics(&metrics);
    if (SUCCEEDED(hr)) {
      return static_cast<int>(std::ceil(metrics.width));
    }
  }

  // 回退到估算值
  return static_cast<int>(text.length() * state.tray_menu.layout.font_size * 0.6);
}

// 计算菜单尺寸
auto calculate_menu_size(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  int total_height = layout.padding * 2;  // 上下边距
  int max_width = layout.min_width;

  for (const auto& item : tray_menu.items) {
    if (item.type == UI::TrayMenu::State::MenuItemType::Separator) {
      total_height += layout.separator_height;
    } else {
      total_height += layout.item_height;

      // 计算文本宽度
      int text_width = calculate_text_width(state, item.text);
      int item_width = text_width + layout.text_padding * 2;

      // 为选中标记预留空间
      if (item.is_checked) {
        item_width += layout.font_size + layout.text_padding;
      }

      max_width = std::max(max_width, item_width);
    }
  }

  tray_menu.menu_size.cx = max_width;
  tray_menu.menu_size.cy = total_height;
}

// 计算菜单位置（确保在屏幕内）
auto calculate_menu_position(Core::State::AppState& state, const Vendor::Windows::POINT& cursor_pos)
    -> Vendor::Windows::POINT {
  auto& tray_menu = state.tray_menu;

  // 获取鼠标所在显示器的工作区
  HMONITOR monitor = MonitorFromPoint({cursor_pos.x, cursor_pos.y}, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoW(monitor, &monitor_info);

  const auto& work_area = monitor_info.rcWork;

  Vendor::Windows::POINT position = cursor_pos;

  // 默认在鼠标右下方显示
  // 检查右边界
  if (position.x + tray_menu.menu_size.cx > work_area.right) {
    position.x = cursor_pos.x - tray_menu.menu_size.cx;  // 显示在左边
  }

  // 检查下边界
  if (position.y + tray_menu.menu_size.cy > work_area.bottom) {
    position.y = cursor_pos.y - tray_menu.menu_size.cy;  // 显示在上边
  }

  // 确保不超出左边界和上边界
  if (position.x < work_area.left) {
    position.x = work_area.left;
  }
  if (position.y < work_area.top) {
    position.y = work_area.top;
  }

  return position;
}

// 根据坐标获取菜单项索引
auto get_menu_item_at_point(const Core::State::AppState& state, const POINT& pt) -> int {
  const auto& tray_menu = state.tray_menu;
  int current_y = tray_menu.layout.padding;

  for (size_t i = 0; i < tray_menu.items.size(); ++i) {
    const auto& item = tray_menu.items[i];
    int item_height = (item.type == UI::TrayMenu::State::MenuItemType::Separator)
                          ? tray_menu.layout.separator_height
                          : tray_menu.layout.item_height;

    if (pt.y >= current_y && pt.y < current_y + item_height &&
        item.type == UI::TrayMenu::State::MenuItemType::Normal) {
      return static_cast<int>(i);
    }
    current_y += item_height;
  }

  return -1;
}

}  // namespace UI::TrayMenu::Layout