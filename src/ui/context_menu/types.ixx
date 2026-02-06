module;

#include <d2d1.h>
#include <windows.h>

#include <iostream>

export module UI.ContextMenu.Types;

import std;
import Features.WindowControl;
import Features.Settings.Menu;

export namespace UI::ContextMenu::Types {

// 菜单项类型枚举
enum class MenuItemType {
  Normal,     // 普通菜单项
  Separator,  // 分隔线
  Submenu     // 子菜单（暂时不实现）
};

// 菜单动作数据结构
struct RatioData {
  size_t index;
  std::wstring name;
  double ratio;
};

struct ResolutionData {
  size_t index;
  std::wstring name;
  std::uint64_t total_pixels;
};

// 业务动作类型 - 类型安全的菜单动作表示
struct MenuAction {
  enum class Type {
    WindowSelection,      // 窗口选择
    RatioSelection,       // 比例选择
    ResolutionSelection,  // 分辨率选择
    FeatureToggle,        // 功能开关
    SystemCommand         // 系统命令
  };

  Type type;
  std::any data;  // 存储具体的业务对象

  // 便捷构造函数
  static auto window_selection(const Features::WindowControl::WindowInfo& window) -> MenuAction {
    return MenuAction{Type::WindowSelection, window};
  }

  static auto ratio_selection(size_t index, const std::wstring& name, double ratio) -> MenuAction {
    return MenuAction{Type::RatioSelection, RatioData{index, name, ratio}};
  }

  static auto resolution_selection(size_t index, const std::wstring& name, std::uint64_t pixels)
      -> MenuAction {
    return MenuAction{Type::ResolutionSelection, ResolutionData{index, name, pixels}};
  }

  static auto feature_toggle(const std::string& action_id) -> MenuAction {
    return MenuAction{Type::FeatureToggle, action_id};
  }

  static auto system_command(const std::string& command) -> MenuAction {
    return MenuAction{Type::SystemCommand, command};
  }
};

// 菜单项结构 - 重构为数据驱动设计
struct MenuItem {
  std::wstring text;
  MenuItemType type = MenuItemType::Normal;
  bool is_checked = false;
  bool is_enabled = true;

  // 核心改进：使用业务动作而不是命令ID
  std::optional<MenuAction> action;

  // 子菜单支持
  std::vector<MenuItem> submenu_items;

  // 构造函数
  MenuItem() = default;

  // 基础文本菜单项
  MenuItem(const std::wstring& text) : text(text) {}

  // 带动作的菜单项
  MenuItem(const std::wstring& text, MenuAction action) : text(text), action(std::move(action)) {}

  // 带选中状态的菜单项
  MenuItem(const std::wstring& text, MenuAction action, bool checked)
      : text(text), is_checked(checked), action(std::move(action)) {}

  // 分隔线构造函数
  static auto separator() -> MenuItem {
    MenuItem item;
    item.type = MenuItemType::Separator;
    return item;
  }

  // 便捷工厂方法
  static auto window_item(const Features::WindowControl::WindowInfo& window) -> MenuItem {
    return MenuItem(window.title, MenuAction::window_selection(window));
  }

  static auto ratio_item(const Features::Settings::Menu::RatioPreset& ratio, size_t index,
                         bool selected = false) -> MenuItem {
    return MenuItem(ratio.name, MenuAction::ratio_selection(index, ratio.name, ratio.ratio),
                    selected);
  }

  static auto resolution_item(const Features::Settings::Menu::ResolutionPreset& resolution,
                              size_t index, bool selected = false) -> MenuItem {
    std::wstring display_text;
    if (resolution.base_width == 0 && resolution.base_height == 0) {
      display_text = resolution.name;
    } else {
      const double megaPixels = resolution.total_pixels / 1000000.0;
      display_text = std::format(L"{} ({:.1f}M)", resolution.name, megaPixels);
    }
    return MenuItem(
        display_text,
        MenuAction::resolution_selection(index, resolution.name, resolution.total_pixels),
        selected);
  }

  static auto feature_item(const std::wstring& text, const std::string& action_id,
                           bool enabled = false) -> MenuItem {
    return MenuItem(text, MenuAction::feature_toggle(action_id), enabled);
  }

  static auto system_item(const std::wstring& text, const std::string& command) -> MenuItem {
    return MenuItem(text, MenuAction::system_command(command));
  }

  // 便捷方法
  auto has_submenu() const -> bool { return !submenu_items.empty(); }
  auto has_action() const -> bool { return action.has_value(); }
};

// 布局配置
struct LayoutConfig {
  // 基础尺寸（96 DPI）
  static constexpr int BASE_ITEM_HEIGHT = 28;
  static constexpr int BASE_SEPARATOR_HEIGHT = 1;
  static constexpr int BASE_PADDING = 8;
  static constexpr int BASE_TEXT_PADDING = 12;
  static constexpr int BASE_MIN_WIDTH = 140;
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

// 交互状态（添加子菜单延迟隐藏）
struct InteractionState {
  int hover_index = -1;
  int submenu_hover_index = -1;
  bool is_mouse_tracking = false;

  // 子菜单显示延迟
  UINT_PTR show_timer_id = 0;
  int pending_submenu_index = -1;
  static constexpr UINT SHOW_TIMER_DELAY = 200;  // 200ms延迟显示
  static constexpr UINT_PTR SHOW_TIMER_ID = 1;

  // 子菜单隐藏延迟（解决对角线移动问题）
  UINT_PTR hide_timer_id = 0;
  static constexpr UINT HIDE_TIMER_DELAY = 300;  // 300ms延迟隐藏
  static constexpr UINT_PTR HIDE_TIMER_ID = 2;
};

}  // namespace UI::ContextMenu::Types