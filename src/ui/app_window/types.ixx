module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <iostream>

export module UI.AppWindow.Types;

import std;
import Features.WindowControl;
import Common.MenuIds;

export namespace UI::AppWindow {

// 菜单项类别枚举（简化版本）
enum class MenuItemCategory { AspectRatio, Resolution, Feature };

// 菜单项结构
struct MenuItem {
  std::wstring text;
  MenuItemCategory category;
  int index;              // 在对应类别中的索引
  std::string action_id;  // 仅 Feature 类别使用

  // 构造函数
  MenuItem(const std::wstring& t, MenuItemCategory cat, int idx, const std::string& action = "")
      : text(t), category(cat), index(idx), action_id(action) {}
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
  int toggle_visibility_hotkey_id = 1;
  int screenshot_hotkey_id = 2;
};

// UI交互状态
struct InteractionState {
  int hover_index = -1;
  size_t current_ratio_index = std::numeric_limits<size_t>::max();
  size_t current_resolution_index = 0;
  bool preview_enabled = false;
  bool overlay_enabled = false;
  bool letterbox_enabled = false;
  bool close_button_hovered = false;
};

// 数据状态（拥有或引用外部数据）
struct DataState {
  std::vector<MenuItem> menu_items;  // 从settings计算生成的菜单项
  std::vector<Features::WindowControl::WindowInfo> windows;
  std::vector<std::wstring> menu_items_to_show;
};

// 渲染相关状态（实际渲染尺寸）
struct LayoutConfig {
  // 实际渲染尺寸（基于DPI缩放和配置）
  int item_height = 24;
  int title_height = 26;
  int separator_height = 1;
  float font_size = 12.0f;  // 改为float，DirectWrite使用浮点数
  int text_padding = 12;
  int indicator_width = 3;
  int ratio_indicator_width = 4;
  int ratio_column_width = 60;
  int resolution_column_width = 120;
  int settings_column_width = 120;

  // 字体大小调整相关常量
  static constexpr float MIN_FONT_SIZE = 8.0f;   // 最小字体大小
  static constexpr float FONT_SIZE_STEP = 0.5f;  // 字体大小调整步长
};

// One Dark Pro风格暗色主题颜色常量 (AppWindow专用)
struct Colors {
  // === One Dark Pro主色调 ===
  static constexpr D2D1_COLOR_F WHITE = {0.16f, 0.17f, 0.21f, 1.0f};      // #282C34 主背景
  static constexpr D2D1_COLOR_F SEPARATOR = {0.20f, 0.22f, 0.27f, 1.0f};  // #333842 分隔线
  static constexpr D2D1_COLOR_F TEXT = {0.87f, 0.91f, 0.98f, 1.0f};       // #DEE8FC 更亮的主文字
  static constexpr D2D1_COLOR_F INDICATOR = {0.38f, 0.68f, 0.84f, 1.0f};  // #61AFEF 青色指示器
  static constexpr D2D1_COLOR_F HOVER = {0.22f, 0.24f, 0.29f, 1.0f};      // #383D4A 悬停背景

  // === 半透明版本（用于分层窗口） ===
  static constexpr D2D1_COLOR_F WHITE_SEMI = {0.16f, 0.17f, 0.21f, 0.65f};      // 主背景
  static constexpr D2D1_COLOR_F SEPARATOR_SEMI = {0.20f, 0.22f, 0.27f, 0.65f};  // 分隔线
  static constexpr D2D1_COLOR_F TITLE_BAR_SEMI = {0.16f, 0.17f, 0.21f, 0.65f};  // 标题栏
  static constexpr D2D1_COLOR_F HOVER_SEMI = {0.22f, 0.24f, 0.29f, 0.65f};      // 悬停背景
};

// AppWindow专用的Direct2D渲染状态
struct RenderContext {
  // Direct2D 1.3资源句柄
  ID2D1Factory7* factory = nullptr;               // Direct2D 1.3 工厂
  ID2D1DCRenderTarget* render_target = nullptr;   // DC渲染目标（兼容性）
  ID2D1DeviceContext6* device_context = nullptr;  // Direct2D 1.3 设备上下文
  IDWriteFactory7* write_factory = nullptr;       // DirectWrite 1.3 工厂

  // 内存DC和位图资源
  HDC memory_dc = nullptr;
  HBITMAP dib_bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  void* bitmap_bits = nullptr;
  SIZE bitmap_size = {0, 0};

  // 缓存的画刷（简单的固定数组，避免动态分配）
  // 不透明画刷（用于文字和指示器）
  ID2D1SolidColorBrush* white_brush = nullptr;
  ID2D1SolidColorBrush* title_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;

  // 半透明画刷（用于背景和装饰元素）
  ID2D1SolidColorBrush* white_semi_brush = nullptr;
  ID2D1SolidColorBrush* title_semi_brush = nullptr;
  ID2D1SolidColorBrush* separator_semi_brush = nullptr;
  ID2D1SolidColorBrush* hover_semi_brush = nullptr;

  // 文本格式
  IDWriteTextFormat* text_format = nullptr;

  // 状态标志
  bool is_initialized = false;
  bool is_rendering = false;
  bool needs_resize = false;
  bool needs_font_update = false;
};

// 辅助函数：将RECT转换为D2D1_RECT_F
inline auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

// 辅助函数：创建D2D矩形
inline auto make_d2d_rect(float left, float top, float right, float bottom) -> D2D1_RECT_F {
  return D2D1::RectF(left, top, right, bottom);
}

}  // namespace UI::AppWindow