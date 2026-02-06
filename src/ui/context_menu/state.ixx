module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

#include <string>

export module UI.ContextMenu.State;

import std;
import UI.ContextMenu.Types;

export namespace UI::ContextMenu::State {

struct ContextMenuState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND submenu_hwnd = nullptr;

  // D2D资源
  ID2D1HwndRenderTarget* render_target = nullptr;
  ID2D1SolidColorBrush* background_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;

  ID2D1HwndRenderTarget* submenu_render_target = nullptr;
  ID2D1SolidColorBrush* submenu_background_brush = nullptr;
  ID2D1SolidColorBrush* submenu_text_brush = nullptr;
  ID2D1SolidColorBrush* submenu_separator_brush = nullptr;
  ID2D1SolidColorBrush* submenu_hover_brush = nullptr;
  ID2D1SolidColorBrush* submenu_indicator_brush = nullptr;

  // 独立的文本格式（DPI 缩放后的字号，不依赖浮窗）
  IDWriteTextFormat* text_format = nullptr;

  bool main_menu_d2d_ready = false;
  bool submenu_d2d_ready = false;

  // 菜单数据和布局
  std::vector<Types::MenuItem> items;
  Types::LayoutConfig layout;
  Types::InteractionState interaction;
  SIZE menu_size{};
  POINT position{};

  // 子菜单状态
  int submenu_parent_index = -1;
  SIZE submenu_size{};
  POINT submenu_position{};

  // 获取当前子菜单项的安全访问方法
  auto get_current_submenu() const -> const std::vector<Types::MenuItem>& {
    if (submenu_parent_index >= 0 && submenu_parent_index < static_cast<int>(items.size()) &&
        items[submenu_parent_index].has_submenu()) {
      return items[submenu_parent_index].submenu_items;
    }
    static const std::vector<Types::MenuItem> empty_submenu;
    return empty_submenu;
  }
};

}  // namespace UI::ContextMenu::State
