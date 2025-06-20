module;

#include <windows.h>

export module Types.State;

import std;
import Core.Constants;
import Types.Presets;
import Features.WindowControl;

export namespace Types::State {

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
  Hide,
  Exit
};

// 菜单项结构
struct MenuItem {
  std::wstring text;
  ItemType type;
  int index;  // 在对应类型中的索引
};

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
  size_t current_ratio_index = std::numeric_limits<size_t>::max();
  size_t current_resolution_index = 0;
  bool preview_enabled = false;
  bool overlay_enabled = false;
  bool letterbox_enabled = false;
};

// 数据状态（拥有或引用外部数据）
struct DataState {
  std::vector<Types::Presets::RatioPreset> ratios;
  std::vector<Types::Presets::ResolutionPreset> resolutions;
  std::vector<MenuItem> menu_items;
  std::vector<Features::WindowControl::WindowInfo> windows;
  const Core::Constants::LocalizedStrings* strings = nullptr;
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

}  // namespace Types::State