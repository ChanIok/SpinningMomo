module;

#include <windows.h>

export module Core.State;

import std;
import Core.Events;
import Core.Constants;
import Common.Types;

export namespace Core::State {

// ============================================================================
// 基础数据类型定义
// ============================================================================

// 菜单项类型枚举
enum class ItemType {
  Ratio,
  Resolution,
  CaptureWindow,
  OpenScreenshot,
  PreviewWindow,
  OverlayWindow,
  LetterboxWindow,
  Reset,
  Close,
  Exit
};

// 菜单项结构
struct MenuItem {
  std::wstring text;
  ItemType type;
  int index;  // 在对应类型中的索引
};

// ============================================================================
// 状态结构定义
// ============================================================================

// 窗口系统状态
struct WindowState {
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
struct UIState {
  int hover_index = -1;
  size_t current_ratio_index = 0;
  size_t current_resolution_index = 0;
  bool preview_enabled = false;
  bool overlay_enabled = false;
  bool letterbox_enabled = false;
};

// 数据状态（非拥有，引用外部数据）
struct DataState {
  std::span<const Common::Types::RatioPreset> ratios;
  std::span<const Common::Types::ResolutionPreset> resolutions;
  std::vector<MenuItem> menu_items;
  const Constants::LocalizedStrings* strings = nullptr;
  std::vector<std::wstring> menu_items_to_show;
};

// 渲染相关状态（DPI缩放后的尺寸）
struct RenderState {
  // 基础尺寸（96 DPI）
  static constexpr int BASE_ITEM_HEIGHT = 24;
  static constexpr int BASE_TITLE_HEIGHT = 26;
  static constexpr int BASE_SEPARATOR_HEIGHT = 1;
  static constexpr int BASE_FONT_SIZE = 12;
  static constexpr int BASE_TEXT_PADDING = 12;
  static constexpr int BASE_INDICATOR_WIDTH = 3;
  static constexpr int BASE_RATIO_INDICATOR_WIDTH = 4;
  static constexpr int BASE_RATIO_COLUMN_WIDTH = 60;
  static constexpr int BASE_RESOLUTION_COLUMN_WIDTH = 120;
  static constexpr int BASE_SETTINGS_COLUMN_WIDTH = 120;

  // DPI缩放后的实际尺寸
  int item_height = BASE_ITEM_HEIGHT;
  int title_height = BASE_TITLE_HEIGHT;
  int separator_height = BASE_SEPARATOR_HEIGHT;
  int font_size = BASE_FONT_SIZE;
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
    font_size = static_cast<int>(BASE_FONT_SIZE * scale);
    text_padding = static_cast<int>(BASE_TEXT_PADDING * scale);
    indicator_width = static_cast<int>(BASE_INDICATOR_WIDTH * scale);
    ratio_indicator_width = static_cast<int>(BASE_RATIO_INDICATOR_WIDTH * scale);
    ratio_column_width = static_cast<int>(BASE_RATIO_COLUMN_WIDTH * scale);
    resolution_column_width = static_cast<int>(BASE_RESOLUTION_COLUMN_WIDTH * scale);
    settings_column_width = static_cast<int>(BASE_SETTINGS_COLUMN_WIDTH * scale);
  }
};

// ============================================================================
// 应用程序完整状态
// ============================================================================

// 应用程序状态（所有状态的聚合）
struct AppState {
  WindowState window;
  UIState ui;
  DataState data;
  RenderState render;
  std::shared_ptr<Core::Events::EventDispatcher> event_dispatcher;

  // 便捷访问方法
  auto is_window_valid() const -> bool { return window.hwnd != nullptr; }
  auto get_total_width() const -> int {
    return render.ratio_column_width + render.resolution_column_width + render.settings_column_width;
  }
  auto get_menu_item_count() const -> size_t { return data.menu_items.size(); }
};

// ============================================================================
// 辅助函数
// ============================================================================

// 根据DPI更新渲染状态
auto update_render_dpi(AppState& state, UINT new_dpi) -> void {
  state.window.dpi = new_dpi;
  state.render.update_dpi_scaling(new_dpi);
}

// 检查项目是否被选中
auto is_item_selected(const MenuItem& item, const UIState& ui_state) -> bool {
  switch (item.type) {
    case ItemType::Ratio:
      return item.index == static_cast<int>(ui_state.current_ratio_index);
    case ItemType::Resolution:
      return item.index == static_cast<int>(ui_state.current_resolution_index);
    case ItemType::PreviewWindow:
      return ui_state.preview_enabled;
    case ItemType::OverlayWindow:
      return ui_state.overlay_enabled;
    case ItemType::LetterboxWindow:
      return ui_state.letterbox_enabled;
    default:
      return false;
  }
}

}  // namespace Core::State 