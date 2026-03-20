module;

export module UI.ContextMenu.State;

import std;
import UI.ContextMenu.Types;
import <d2d1.h>;
import <dwrite.h>;
import <windows.h>;

export namespace UI::ContextMenu::State {

struct RenderSurface {
  ID2D1DCRenderTarget* render_target = nullptr;
  HDC memory_dc = nullptr;
  HBITMAP dib_bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  void* bitmap_bits = nullptr;
  SIZE bitmap_size{};

  ID2D1SolidColorBrush* background_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;

  bool is_ready = false;
};

struct ContextMenuState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND submenu_hwnd = nullptr;

  // D2D资源
  RenderSurface main_surface;
  RenderSurface submenu_surface;

  // 独立的文本格式（DPI 缩放后的字号，不依赖浮窗）
  IDWriteTextFormat* text_format = nullptr;

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
