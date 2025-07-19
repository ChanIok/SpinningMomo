module;

export module Common.MenuIds;

import std;

export namespace Common::MenuIds {

// 菜单项ID枚举 - 强类型化的菜单标识符
enum class Id {
  ScreenshotCapture,
  ScreenshotOpenFolder,
  FeatureTogglePreview,
  FeatureToggleOverlay,
  FeatureToggleLetterbox,
  WindowResetTransform,
  PanelHide,
  AppExit
};

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
    case Id::WindowResetTransform:
      return "window.reset_transform";
    case Id::PanelHide:
      return "panel.hide";
    case Id::AppExit:
      return "app.exit";
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
  if (str == "window.reset_transform") return Id::WindowResetTransform;
  if (str == "panel.hide") return Id::PanelHide;
  if (str == "app.exit") return Id::AppExit;
  return std::nullopt;
}

}  // namespace Common::MenuIds