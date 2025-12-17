module;

export module Features.Settings.Menu;

import std;

namespace Features::Settings::State {
export struct SettingsState;
}

namespace Features::Settings::Menu {

// === Types (Merged from menu_data/types.ixx) ===

// 比例预设
export struct RatioPreset {
  std::wstring name;  // 比例名称
  double ratio;       // 宽高比值

  constexpr RatioPreset(const std::wstring& n, double r) noexcept : name(n), ratio(r) {}
};

// 分辨率预设
export struct ResolutionPreset {
  std::wstring name;           // 显示名称（如 "4K"）
  std::uint64_t total_pixels;  // 总像素数
  int base_width;              // 基准宽度
  int base_height;             // 基准高度

  constexpr ResolutionPreset(const std::wstring& n, int w, int h) noexcept
      : name(n), total_pixels(static_cast<std::uint64_t>(w) * h), base_width(w), base_height(h) {}
};

// 计算后的功能项
export struct ComputedFeatureItem {
  std::wstring text;      // 显示文本
  std::string action_id;  // 操作标识
  bool enabled;           // 是否启用
  int order;              // 排序

  ComputedFeatureItem(const std::wstring& t, const std::string& id, bool en = true, int ord = 0)
      : text(t), action_id(id), enabled(en), order(ord) {}
};

// === Ids (Merged from menu_ids/menu_ids.ixx) ===

// 菜单项ID枚举
export enum class Id : int {
  // === 应用核心 (1000-1099) ===
  AppExit = 1000,
  AppUserGuide = 1001,
  AppWebView = 1002,
  PanelHide = 1003,

  // === 窗口控制模块 (2000-2999) ===
  WindowControlWindowBase = 2000,
  WindowControlRatioBase = 2200,
  WindowControlResolutionBase = 2400,
  WindowControlResetTransform = 2600,
  WindowControlToggleBorder = 2601,

  // === 截图功能 (3000-3099) ===
  ScreenshotCapture = 3000,
  ScreenshotOpenFolder = 3001,

  // === 功能特性切换 (3100-3199) ===
  FeatureTogglePreview = 3100,
  FeatureToggleOverlay = 3101,
  FeatureToggleLetterbox = 3102,
  FeatureToggleRecording = 3103,

  // === 设置相关 (3200-3299) ===
  SettingsConfig = 3200,
  SettingsHotkey = 3201
};

// 转换为字符串视图
export constexpr std::string_view to_string(Id id) {
  switch (id) {
    case Id::ScreenshotCapture:
      return "screenshot.capture";
    case Id::ScreenshotOpenFolder:
      return "screenshot.open_folder";
    case Id::FeatureTogglePreview:
      return "feature.toggle_preview";
    case Id::FeatureToggleOverlay:
      return "feature.toggle_overlay";
    case Id::FeatureToggleLetterbox:
      return "feature.toggle_letterbox";
    case Id::FeatureToggleRecording:
      return "feature.toggle_recording";
    case Id::WindowControlResetTransform:
      return "window.reset_transform";
    case Id::PanelHide:
      return "panel.hide";
    case Id::AppExit:
      return "app.exit";
    default:
      return "";
  }
}

// 从字符串转换为ID
export constexpr std::optional<Id> from_string(std::string_view str) {
  if (str == "screenshot.capture") return Id::ScreenshotCapture;
  if (str == "screenshot.open_folder") return Id::ScreenshotOpenFolder;
  if (str == "feature.toggle_preview") return Id::FeatureTogglePreview;
  if (str == "feature.toggle_overlay") return Id::FeatureToggleOverlay;
  if (str == "feature.toggle_letterbox") return Id::FeatureToggleLetterbox;
  if (str == "feature.toggle_recording") return Id::FeatureToggleRecording;
  if (str == "window.reset_transform") return Id::WindowControlResetTransform;
  if (str == "panel.hide") return Id::PanelHide;
  if (str == "app.exit") return Id::AppExit;
  return std::nullopt;
}

// === Getters Interface ===

// 获取当前的比例预设数据
export auto get_ratios(const Features::Settings::State::SettingsState& state)
    -> const std::vector<RatioPreset>&;

// 获取当前的分辨率预设数据
export auto get_resolutions(const Features::Settings::State::SettingsState& state)
    -> const std::vector<ResolutionPreset>&;

// 获取当前的功能项数据
export auto get_feature_items(const Features::Settings::State::SettingsState& state)
    -> const std::vector<ComputedFeatureItem>&;

}  // namespace Features::Settings::Menu
