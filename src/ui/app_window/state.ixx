module;

#include <windows.h>

export module UI.AppWindow.State;

import std;
import Core.Constants;
import Types.Presets;
import Features.WindowControl;

export namespace UI::AppWindow {

// 菜单项类型枚举
enum class ItemType {
  None,
  // 截图
  ScreenshotCapture,
  ScreenshotOpenFolder,
  // 功能
  FeatureTogglePreview,
  FeatureToggleOverlay,
  FeatureToggleLetterbox,
  // 窗口
  WindowResetTransform,
  // 面板
  PanelHide,
  // 应用
  AppExit,
  // 比例
  AspectRatio,
  // 分辨率
  Resolution
};

// 布局常量
constexpr int BASE_ITEM_HEIGHT = 24;
constexpr int BASE_TITLE_HEIGHT = 26;
constexpr int BASE_SEPARATOR_HEIGHT = 1;
constexpr int BASE_FONT_SIZE = 12;
constexpr int BASE_TEXT_PADDING = 12;
constexpr int BASE_INDICATOR_WIDTH = 3;
constexpr int BASE_RATIO_INDICATOR_WIDTH = 4;
constexpr int BASE_RATIO_COLUMN_WIDTH = 60;
constexpr int BASE_RESOLUTION_COLUMN_WIDTH = 120;
constexpr int BASE_SETTINGS_COLUMN_WIDTH = 120;

// 菜单项结构
struct MenuItem {
  std::wstring text;
  ItemType type;
  int index;  // 在对应类型中的索引
};

// 窗口系统状态
struct WindowInfo {
  HWND hwnd = nullptr;
  HINSTANCE instance = nullptr;
  SIZE size{};
  POINT position{};
  UINT dpi = 96;
  bool is_visible = false;
  bool is_tracking_mouse = false;
  int hotkey_id = 1;
};

// UI交互状态
struct InteractionState {
  int hover_index = -1;
  size_t current_ratio_index = std::numeric_limits<size_t>::max();
  size_t current_resolution_index = 0;
  bool preview_enabled = false;
  bool overlay_enabled = false;
  bool letterbox_enabled = false;
};

// 数据状态（拥有或引用外部数据）
struct DataState {
  std::vector<MenuItem> menu_items;  // 从settings计算生成的菜单项
  std::vector<Features::WindowControl::WindowInfo> windows;
  const Core::Constants::LocalizedStrings* strings = nullptr;
  std::vector<std::wstring> menu_items_to_show;
};

// 渲染状态（分离可变的渲染状态，解决const_cast问题）
struct RenderState {
  bool is_rendering = false;
  bool needs_resize = false;
};

// 渲染相关状态（DPI缩放后的尺寸）
struct LayoutConfig {
  // DPI缩放后的实际尺寸
  int item_height = BASE_ITEM_HEIGHT;
  int title_height = BASE_TITLE_HEIGHT;
  int separator_height = BASE_SEPARATOR_HEIGHT;
  float font_size = BASE_FONT_SIZE;  // 改为float，DirectWrite使用浮点数
  int text_padding = BASE_TEXT_PADDING;
  int indicator_width = BASE_INDICATOR_WIDTH;
  int ratio_indicator_width = BASE_RATIO_INDICATOR_WIDTH;
  int ratio_column_width = BASE_RATIO_COLUMN_WIDTH;
  int resolution_column_width = BASE_RESOLUTION_COLUMN_WIDTH;
  int settings_column_width = BASE_SETTINGS_COLUMN_WIDTH;

  // 更新DPI缩放
  auto update_dpi_scaling(UINT dpi) -> void {
    const double scale = static_cast<double>(dpi) / 96.0;
    item_height = static_cast<int>(BASE_ITEM_HEIGHT * scale);
    title_height = static_cast<int>(BASE_TITLE_HEIGHT * scale);
    separator_height = static_cast<int>(BASE_SEPARATOR_HEIGHT * scale);
    font_size = static_cast<float>(BASE_FONT_SIZE * scale);  // 使用float
    text_padding = static_cast<int>(BASE_TEXT_PADDING * scale);
    indicator_width = static_cast<int>(BASE_INDICATOR_WIDTH * scale);
    ratio_indicator_width = static_cast<int>(BASE_RATIO_INDICATOR_WIDTH * scale);
    ratio_column_width = static_cast<int>(BASE_RATIO_COLUMN_WIDTH * scale);
    resolution_column_width = static_cast<int>(BASE_RESOLUTION_COLUMN_WIDTH * scale);
    settings_column_width = static_cast<int>(BASE_SETTINGS_COLUMN_WIDTH * scale);
  }
};

// 主窗口聚合状态
struct State {
  WindowInfo window;
  InteractionState ui;
  DataState data;
  LayoutConfig layout;
  RenderState render;
};

// 辅助函数
auto is_item_selected(const MenuItem& item, const InteractionState& ui_state) -> bool {
  switch (item.type) {
    case ItemType::AspectRatio:
      return item.index == static_cast<int>(ui_state.current_ratio_index);
    case ItemType::Resolution:
      return item.index == static_cast<int>(ui_state.current_resolution_index);
    case ItemType::FeatureTogglePreview:
      return ui_state.preview_enabled;
    case ItemType::FeatureToggleOverlay:
      return ui_state.overlay_enabled;
    case ItemType::FeatureToggleLetterbox:
      return ui_state.letterbox_enabled;
    default:
      return false;
  }
}

}  // namespace UI::AppWindow