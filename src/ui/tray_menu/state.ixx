module;

#include <d2d1.h>
#include <windows.h>

#include <iostream>

export module UI.TrayMenu.State;

import std;
import UI.TrayMenu.Types;

export namespace UI::TrayMenu::State {

// 托盘菜单数据
struct TrayMenuState {
  // 窗口相关
  HWND hwnd = nullptr;
  bool is_visible = false;
  bool is_created = false;

  // 主菜单D2D资源
  ID2D1HwndRenderTarget* render_target = nullptr;
  ID2D1SolidColorBrush* white_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;

  // 子菜单D2D资源（新增专用画刷）
  ID2D1HwndRenderTarget* submenu_render_target = nullptr;
  ID2D1SolidColorBrush* submenu_white_brush = nullptr;
  ID2D1SolidColorBrush* submenu_text_brush = nullptr;
  ID2D1SolidColorBrush* submenu_separator_brush = nullptr;
  ID2D1SolidColorBrush* submenu_hover_brush = nullptr;
  ID2D1SolidColorBrush* submenu_indicator_brush = nullptr;

  // 简化的初始化标志
  bool main_menu_d2d_ready = false;
  bool submenu_d2d_ready = false;

  // 菜单数据
  std::vector<Types::MenuItem> items;

  // 布局和交互
  Types::LayoutConfig layout;
  Types::InteractionState interaction;

  // 计算出的尺寸
  SIZE menu_size{};
  POINT position{};

  // 子菜单相关
  HWND submenu_hwnd = nullptr;    // 当前显示的子菜单窗口
  int submenu_parent_index = -1;  // 触发子菜单的父项索引
  SIZE submenu_size{};            // 子菜单尺寸
  POINT submenu_position{};       // 子菜单位置

  // 便捷方法
  auto is_valid() const -> bool { return hwnd != nullptr && is_created; }

  auto get_item_count() const -> size_t { return items.size(); }

  auto get_normal_item_count() const -> size_t {
    return std::count_if(items.begin(), items.end(), [](const Types::MenuItem& item) {
      return item.type == Types::MenuItemType::Normal;
    });
  }

  // 获取当前子菜单项的安全访问方法
  auto get_current_submenu() const -> const std::vector<Types::MenuItem>& {
    if (submenu_parent_index >= 0 && submenu_parent_index < static_cast<int>(items.size()) &&
        items[submenu_parent_index].has_submenu()) {
      return items[submenu_parent_index].submenu_items;
    }
    static const std::vector<Types::MenuItem> empty_submenu;
    return empty_submenu;
  }

  // 检查是否有活跃的子菜单
  auto has_active_submenu() const -> bool {
    return submenu_parent_index >= 0 && submenu_parent_index < static_cast<int>(items.size()) &&
           items[submenu_parent_index].has_submenu();
  }
};

}  // namespace UI::TrayMenu::State
