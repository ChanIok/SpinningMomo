module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

#include <string>

export module UI.FloatingWindow.Types;

import std;
import Features.WindowControl;
import Features.Settings.Menu;

export namespace UI::FloatingWindow {

// 菜单布局模式
enum class MenuLayoutMode {
  AutoHeight,  // 自适应高度：高度由最大列决定
  Paged        // 翻页模式：固定高度 + 独立列翻页
};

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
};

// UI交互状态
struct InteractionState {
  int hover_index = -1;
  size_t current_ratio_index = std::numeric_limits<size_t>::max();
  size_t current_resolution_index = 0;
  bool close_button_hovered = false;

  // 翻页模式状态（仅 Paged 模式使用）
  size_t ratio_scroll_offset = 0;
  size_t resolution_scroll_offset = 0;
  size_t feature_scroll_offset = 0;
  int hovered_column = -1;  // -1: 无, 0: 比例列, 1: 分辨率列, 2: 功能列
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
  int scroll_indicator_width = 2;  // 滚动条宽度

  // 字体大小调整相关常量
  static constexpr float MIN_FONT_SIZE = 8.0f;   // 最小字体大小
  static constexpr float FONT_SIZE_STEP = 0.5f;  // 字体大小调整步长

  // 翻页模式配置
  MenuLayoutMode layout_mode = MenuLayoutMode::Paged;
  static constexpr int MAX_VISIBLE_ROWS = 7;  // 翻页模式下每页最大可见行数
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
  ID2D1SolidColorBrush* background_brush = nullptr;
  ID2D1SolidColorBrush* title_brush = nullptr;
  ID2D1SolidColorBrush* separator_brush = nullptr;
  ID2D1SolidColorBrush* text_brush = nullptr;
  ID2D1SolidColorBrush* indicator_brush = nullptr;
  ID2D1SolidColorBrush* hover_brush = nullptr;
  ID2D1SolidColorBrush* scroll_indicator_brush = nullptr;  // 滚动条画刷

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

}  // namespace UI::FloatingWindow