#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/dcomp.hpp"
#include "vendor/windows/dwrite_3.hpp"
#include "vendor/windows/dxgi1_2.hpp"

#include "ui/context_menu/types.hpp"

namespace ui::context_menu {

struct RenderResources {
  wil::com_ptr<IDXGISwapChain1> swap_chain;
  wil::com_ptr<IDCompositionTarget> composition_target;
  wil::com_ptr<IDCompositionVisual> composition_visual;
  wil::com_ptr<ID2D1DeviceContext6> device_context;
  wil::com_ptr<ID2D1Bitmap1> target_bitmap;
  SIZE surface_size{};

  wil::com_ptr<ID2D1SolidColorBrush> background_brush;
  wil::com_ptr<ID2D1SolidColorBrush> text_brush;
  wil::com_ptr<ID2D1SolidColorBrush> separator_brush;
  wil::com_ptr<ID2D1SolidColorBrush> hover_brush;
  wil::com_ptr<ID2D1SolidColorBrush> indicator_brush;

  bool is_ready = false;
};

struct ContextMenuState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND submenu_hwnd = nullptr;

  // D2D资源
  RenderResources main_render_resources;
  RenderResources submenu_render_resources;

  // 独立的文本格式（DPI 缩放后的字号，不依赖浮窗）
  wil::com_ptr<IDWriteTextFormat> text_format;

  // 菜单数据和布局
  std::vector<MenuItem> items;
  LayoutConfig layout;
  InteractionState interaction;
  MenuOpenAnimation main_animation;
  MenuOpenAnimation submenu_animation;
  SIZE menu_size{};
  POINT position{};

  // 子菜单状态
  int submenu_parent_index = -1;
  SIZE submenu_size{};
  POINT submenu_position{};

  // 获取当前子菜单项的安全访问方法
  inline auto get_current_submenu() const -> const std::vector<MenuItem>& {
    if (submenu_parent_index >= 0 && submenu_parent_index < static_cast<int>(items.size()) &&
        items[submenu_parent_index].has_submenu()) {
      return items[submenu_parent_index].submenu_items;
    }
    static const std::vector<MenuItem> empty_submenu;
    return empty_submenu;
  }
};

}  // namespace ui::context_menu
