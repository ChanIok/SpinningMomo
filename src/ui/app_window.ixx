module;

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include <windowsx.h>
#include <limits>

export module UI.AppWindow;

import std;
import Core.Constants;
import Common.Types;

// 使用现代C++的类型别名
using AspectRatio = Common::Types::RatioPreset;
using ResolutionPreset = Common::Types::ResolutionPreset;
using LocalizedStrings = Constants::LocalizedStrings;

// 应用程序主窗口类
export class AppWindow {
 public:
  // 列表项类型
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

  // 列表项结构
  struct MenuItem {
    std::wstring text;
    ItemType type;
    int index;  // 在对应类型中的索引
  };

  explicit AppWindow(HINSTANCE hInstance);
  ~AppWindow() = default;

  // 禁用拷贝和移动
  AppWindow(const AppWindow&) = delete;
  auto operator=(const AppWindow&) -> AppWindow& = delete;
  AppWindow(AppWindow&&) = delete;
  auto operator=(AppWindow&&) -> AppWindow& = delete;

  // 现代化的创建方法，使用 std::expected 进行错误处理
  [[nodiscard]] auto Create(std::span<const AspectRatio> ratios,
                            std::span<const ResolutionPreset> resolutions,
                            const LocalizedStrings& strings, size_t currentRatioIndex,
                            size_t currentResolutionIndex, bool previewEnabled, bool overlayEnabled,
                            bool letterboxEnabled) -> std::expected<void, std::wstring>;

  auto Show() -> void;
  auto Hide() -> void;
  [[nodiscard]] auto IsVisible() const -> bool;
  auto ToggleVisibility() -> void;
  auto SetCurrentRatio(size_t index) -> void;
  auto SetCurrentResolution(size_t index) -> void;
  auto SetPreviewEnabled(bool enabled) -> void;
  auto SetOverlayEnabled(bool enabled) -> void;
  auto SetLetterboxEnabled(bool enabled) -> void;
  auto SetMenuItemsToShow(std::span<const std::wstring> items) -> void;
  auto UpdateMenuItems(const LocalizedStrings& strings, bool forceRedraw = true) -> void;
  [[nodiscard]] auto GetHwnd() const -> HWND;
  auto Activate() -> void;
  auto RegisterHotkey(UINT modifiers, UINT key) -> bool;
  auto UnregisterHotkey() -> void;

 private:
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

  // DPI相关的尺寸变量
  UINT m_dpi = 96;
  int m_itemHeight = BASE_ITEM_HEIGHT;
  int m_titleHeight = BASE_TITLE_HEIGHT;
  int m_separatorHeight = BASE_SEPARATOR_HEIGHT;
  int m_fontSize = BASE_FONT_SIZE;
  int m_textPadding = BASE_TEXT_PADDING;
  int m_indicatorWidth = BASE_INDICATOR_WIDTH;
  int m_ratioIndicatorWidth = BASE_RATIO_INDICATOR_WIDTH;
  int m_ratioColumnWidth = BASE_RATIO_COLUMN_WIDTH;
  int m_resolutionColumnWidth = BASE_RESOLUTION_COLUMN_WIDTH;
  int m_settingsColumnWidth = BASE_SETTINGS_COLUMN_WIDTH;

  // 窗口相关成员
  HWND m_hwnd = nullptr;
  HINSTANCE m_hInstance = nullptr;
  int m_hotkeyId = 1;

  // 状态成员
  int m_hoverIndex = -1;
  size_t m_currentRatioIndex = std::numeric_limits<size_t>::max();
  size_t m_currentResolutionIndex = std::numeric_limits<size_t>::max();
  bool m_previewEnabled = false;
  bool m_overlayEnabled = false;
  bool m_letterboxEnabled = false;

  // 数据成员 - 使用现代C++的span来表示非拥有的数据视图
  std::span<const AspectRatio> m_ratioItems;
  std::span<const ResolutionPreset> m_resolutionItems;
  std::vector<MenuItem> m_items;
  const LocalizedStrings* m_strings = nullptr;
  std::vector<std::wstring> m_menuItemsToShow;

  // 私有方法
  auto InitializeItems(const LocalizedStrings& strings) -> void;
  auto RegisterWindowClass() -> void;
  static LRESULT CALLBACK AppWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  auto HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT;
  auto OnPaint(HDC hdc) -> void;
  auto OnMouseMove(int x, int y) -> void;
  auto OnMouseLeave() -> void;
  auto OnLButtonDown(int x, int y) -> void;
  auto UpdateDpiDependentResources() -> void;
  [[nodiscard]] auto CalculateWindowHeight() const -> int;
  [[nodiscard]] auto GetItemIndexFromPoint(int x, int y) const -> int;
};
