module;

export module Common.MenuIds;

import std;

export namespace Common::MenuIds {

// 菜单项ID枚举 - 强类型化的菜单标识符，按功能模块重新组织
enum class Id : int {
  // === 应用核心 (1000-1099) ===
  AppExit = 1000,
  AppUserGuide = 1001,
  AppWebView = 1002,
  PanelHide = 1003,  // 应用面板的隐藏功能

  // === 窗口控制模块 (2000-2999) ===
  // 窗口选择子范围: 2000-2199 (200个窗口)
  WindowControlWindowBase = 2000,

  // 比例选择子范围: 2200-2399 (200个比例)
  WindowControlRatioBase = 2200,

  // 分辨率选择子范围: 2400-2599 (200个分辨率)
  WindowControlResolutionBase = 2400,

  // 窗口操作功能: 2600-2699
  WindowControlResetTransform = 2600,
  WindowControlToggleBorder = 2601,

  // === 截图功能 (3000-3099) ===
  ScreenshotCapture = 3000,
  ScreenshotOpenFolder = 3001,

  // === 功能特性切换 (3100-3199) ===
  FeatureTogglePreview = 3100,    // 预览窗口
  FeatureToggleOverlay = 3101,    // 叠加层
  FeatureToggleLetterbox = 3102,  // 黑边模式
  FeatureToggleRecording = 3103,  // 录屏

  // === 设置相关 (3200-3299) ===
  SettingsConfig = 3200,
  SettingsHotkey = 3201
};

// 获取数值ID的便捷函数
constexpr int to_int(Id id) { return static_cast<int>(id); }

// 转换为字符串视图
constexpr std::string_view to_string(Id id) {
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
    // 基础范围和设置项不需要字符串表示
    case Id::WindowControlWindowBase:
    case Id::WindowControlRatioBase:
    case Id::WindowControlResolutionBase:
    case Id::WindowControlToggleBorder:
    case Id::AppUserGuide:
    case Id::AppWebView:
    case Id::SettingsConfig:
    case Id::SettingsHotkey:
      return "";
  }
  return "";
}

// 从字符串转换为ID（用于配置解析等）
constexpr std::optional<Id> from_string(std::string_view str) {
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

}  // namespace Common::MenuIds