module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>

module UI.ContextMenu.Layout;

import std;
import Core.State;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.AppWindow.State;
import UI.AppWindow.Types;
import Vendor.Windows;

namespace UI::ContextMenu::Layout {

auto calculate_text_width(const Core::State::AppState& state, const std::wstring& text) -> int {
  const auto& menu_state = *state.context_menu;
  if (!state.app_window) {
    return static_cast<int>(text.length() * menu_state.layout.font_size * 0.6);
  }
  const auto& d2d = state.app_window->d2d_context;
  if (!d2d.is_initialized || !d2d.text_format) {
    return static_cast<int>(text.length() * menu_state.layout.font_size * 0.6);
  }

  Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
  HRESULT hr = d2d.write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), d2d.text_format, 1000.0f,
      static_cast<float>(menu_state.layout.item_height), &text_layout);

  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS metrics;
    if (SUCCEEDED(text_layout->GetMetrics(&metrics))) {
      return static_cast<int>(std::ceil(metrics.width));
    }
  }
  return static_cast<int>(text.length() * menu_state.layout.font_size * 0.6);
}

auto calculate_menu_size(Core::State::AppState& state) -> void {
  auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  int total_height = layout.padding * 2;
  int max_width = layout.min_width;

  for (const auto& item : menu_state.items) {
    if (item.type == Types::MenuItemType::Separator) {
      total_height += layout.separator_height;
    } else {
      total_height += layout.item_height;
      int text_width = calculate_text_width(state, item.text);
      int item_width = text_width + layout.text_padding * 2;
      if (item.is_checked) {
        item_width += layout.font_size + layout.text_padding;
      }
      max_width = std::max(max_width, item_width);
    }
  }
  menu_state.menu_size = {max_width, total_height};
}

auto calculate_menu_position(const Core::State::AppState& state,
                             const Vendor::Windows::POINT& cursor_pos) -> Vendor::Windows::POINT {
  const auto& menu_state = *state.context_menu;
  HMONITOR monitor = MonitorFromPoint({cursor_pos.x, cursor_pos.y}, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{sizeof(MONITORINFO)};
  GetMonitorInfoW(monitor, &monitor_info);
  const auto& work_area = monitor_info.rcWork;

  Vendor::Windows::POINT position = cursor_pos;
  if (position.x + menu_state.menu_size.cx > work_area.right) {
    position.x = cursor_pos.x - menu_state.menu_size.cx;
  }
  if (position.y + menu_state.menu_size.cy > work_area.bottom) {
    position.y = cursor_pos.y - menu_state.menu_size.cy;
  }
  position.x = std::max(position.x, work_area.left);
  position.y = std::max(position.y, work_area.top);
  return position;
}

auto get_menu_item_at_point(const Core::State::AppState& state, const POINT& pt) -> int {
  const auto& menu_state = *state.context_menu;
  int current_y = menu_state.layout.padding;
  for (size_t i = 0; i < menu_state.items.size(); ++i) {
    const auto& item = menu_state.items[i];
    int item_height = (item.type == Types::MenuItemType::Separator)
                          ? menu_state.layout.separator_height
                          : menu_state.layout.item_height;
    if (pt.y >= current_y && pt.y < current_y + item_height &&
        item.type == Types::MenuItemType::Normal) {
      return static_cast<int>(i);
    }
    current_y += item_height;
  }
  return -1;
}

// 计算子菜单尺寸
auto calculate_submenu_size(Core::State::AppState& state) -> void {
  auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;
  const auto& current_submenu = menu_state.get_current_submenu();

  if (current_submenu.empty()) {
    menu_state.submenu_size = {0, 0};
    return;
  }

  // 计算子菜单的宽度和高度
  int max_width = layout.min_width;
  int total_height = layout.padding * 2;

  for (const auto& item : current_submenu) {
    if (item.type == Types::MenuItemType::Separator) {
      total_height += layout.separator_height;
    } else {
      total_height += layout.item_height;
      // 使用现有的文本宽度计算函数，比tray_menu的简单估算更精确
      int text_width = calculate_text_width(state, item.text) + layout.text_padding * 2;
      if (item.is_checked) {
        text_width += layout.font_size + layout.text_padding;
      }
      max_width = std::max(max_width, text_width);
    }
  }

  menu_state.submenu_size = {max_width, total_height};
}

// 计算子菜单位置
auto calculate_submenu_position(Core::State::AppState& state, int parent_index) -> void {
  auto& menu_state = *state.context_menu;
  const auto& layout = menu_state.layout;

  // 计算父菜单项的位置
  int parent_y = layout.padding;
  for (int i = 0; i < parent_index; ++i) {
    const auto& item = menu_state.items[i];
    if (item.type == Types::MenuItemType::Separator) {
      parent_y += layout.separator_height;
    } else {
      parent_y += layout.item_height;
    }
  }

  // 子菜单显示在主菜单右侧
  menu_state.submenu_position.x = menu_state.position.x + menu_state.menu_size.cx;
  menu_state.submenu_position.y = menu_state.position.y + parent_y;

  // 确保子菜单不会超出屏幕边界，使用与主菜单一致的监视器检测方式
  HMONITOR monitor =
      MonitorFromPoint({menu_state.position.x, menu_state.position.y}, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{sizeof(MONITORINFO)};
  GetMonitorInfoW(monitor, &monitor_info);
  const auto& work_area = monitor_info.rcWork;

  // 检查右边界
  if (menu_state.submenu_position.x + menu_state.submenu_size.cx > work_area.right) {
    // 显示在主菜单左侧
    menu_state.submenu_position.x = menu_state.position.x - menu_state.submenu_size.cx;
  }

  // 检查下边界
  if (menu_state.submenu_position.y + menu_state.submenu_size.cy > work_area.bottom) {
    menu_state.submenu_position.y = work_area.bottom - menu_state.submenu_size.cy;
  }

  // 检查上边界
  if (menu_state.submenu_position.y < work_area.top) {
    menu_state.submenu_position.y = work_area.top;
  }
}

}  // namespace UI::ContextMenu::Layout