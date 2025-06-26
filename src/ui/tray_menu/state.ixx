module;

#include <d2d1.h>
#include <windows.h>

#include <iostream>

export module UI.TrayMenu.State;

import std;

export namespace UI::TrayMenu::State {

// 菜单项类型枚举
enum class MenuItemType {
  Normal,     // 普通菜单项
  Separator,  // 分隔线
  Submenu     // 子菜单（暂时不实现）
};

// 菜单项结构
struct MenuItem {
  std::wstring text;
  int command_id = 0;
  MenuItemType type = MenuItemType::Normal;
  bool is_checked = false;
  bool is_enabled = true;

  // 构造函数
  MenuItem() = default;
  MenuItem(const std::wstring& text, int id) : text(text), command_id(id) {}
  MenuItem(const std::wstring& text, int id, bool checked)
      : text(text), command_id(id), is_checked(checked) {}

  // 分隔线构造函数
  static auto separator() -> MenuItem {
    MenuItem item;
    item.type = MenuItemType::Separator;
    return item;
  }
};

// 布局配置
struct LayoutConfig {
  // 基础尺寸（96 DPI）
  static constexpr int BASE_ITEM_HEIGHT = 28;
  static constexpr int BASE_SEPARATOR_HEIGHT = 1;
  static constexpr int BASE_PADDING = 8;
  static constexpr int BASE_TEXT_PADDING = 12;
  static constexpr int BASE_MIN_WIDTH = 180;
  static constexpr int BASE_FONT_SIZE = 12;
  static constexpr int BASE_BORDER_RADIUS = 6;

  // DPI缩放后的尺寸
  UINT dpi = 96;
  int item_height = BASE_ITEM_HEIGHT;
  int separator_height = BASE_SEPARATOR_HEIGHT;
  int padding = BASE_PADDING;
  int text_padding = BASE_TEXT_PADDING;
  int min_width = BASE_MIN_WIDTH;
  int font_size = BASE_FONT_SIZE;
  int border_radius = BASE_BORDER_RADIUS;

  auto update_dpi_scaling(UINT new_dpi) -> void {
    dpi = new_dpi;
    const double scale = static_cast<double>(new_dpi) / 96.0;
    item_height = static_cast<int>(BASE_ITEM_HEIGHT * scale);
    separator_height = static_cast<int>(BASE_SEPARATOR_HEIGHT * scale);
    padding = static_cast<int>(BASE_PADDING * scale);
    text_padding = static_cast<int>(BASE_TEXT_PADDING * scale);
    min_width = static_cast<int>(BASE_MIN_WIDTH * scale);
    font_size = static_cast<int>(BASE_FONT_SIZE * scale);
    border_radius = static_cast<int>(BASE_BORDER_RADIUS * scale);
  }
};

// 交互状态
struct InteractionState {
  int hover_index = -1;
  bool is_mouse_tracking = false;
  POINT last_mouse_pos{};
};

// 托盘菜单数据
struct Data {
  // 窗口相关
  HWND hwnd = nullptr;
  bool is_visible = false;
  bool is_created = false;

  // D2D渲染目标（托盘菜单专用）
  ID2D1HwndRenderTarget* render_target = nullptr;
  bool render_target_initialized = false;

  // 托盘菜单专用画刷（与渲染目标生命周期绑定）
  ID2D1SolidColorBrush* white_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;
  bool brushes_initialized = false;

  // 菜单数据
  std::vector<MenuItem> items;

  // 布局和交互
  LayoutConfig layout;
  InteractionState interaction;

  // 计算出的尺寸
  SIZE menu_size{};
  POINT position{};

  // 便捷方法
  auto is_valid() const -> bool { return hwnd != nullptr && is_created; }

  auto get_item_count() const -> size_t { return items.size(); }

  auto get_normal_item_count() const -> size_t {
    return std::count_if(items.begin(), items.end(),
                         [](const MenuItem& item) { return item.type == MenuItemType::Normal; });
  }
};

}  // namespace UI::TrayMenu::State
