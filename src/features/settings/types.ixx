module;

export module Features.Settings.Types;

import std;
import Core.Constants;
import Utils.String;
import Common.MenuIds;

export namespace Features::Settings::Types {

// 功能项（应用菜单中的功能）
struct FeatureItem {
  std::string id;       // 如: "CaptureWindow", "OpenScreenshot"
  std::string label;    // 显示名称
  bool enabled = true;  // 是否显示在菜单中
  int order = 0;        // 排序序号
};

// 预设项（比例/分辨率）
struct PresetItem {
  std::string id;          // 如: "16:9", "1080P"
  std::string label;       // 显示名称
  bool enabled = true;     // 是否显示在菜单中
  int order = 0;           // 排序序号
  bool is_custom = false;  // 是否为自定义项
};

// 应用菜单配置
struct AppMenuSettings {
  std::vector<FeatureItem> feature_items;
  std::vector<PresetItem> aspect_ratios;
  std::vector<PresetItem> resolutions;
};

// 完整的应用设置（统一的核心类型）
struct AppSettings {
  std::string version = "1.0";
  std::string title;  // 窗口标题
  AppMenuSettings app_menu;
};

// 设置变更事件数据（暂时保留以维持兼容性）
struct SettingsChangeData {
  AppSettings old_settings;
  AppSettings new_settings;
  std::string change_description;
};

// === 简化的RPC接口 ===

// 获取设置
struct GetSettingsParams {
  // 空结构体，未来可扩展
};

using GetSettingsResult = AppSettings;

using UpdateSettingsParams = AppSettings;

struct UpdateSettingsResult {
  bool success;
  std::string message;
};

// 业务错误类型
struct ServiceError {
  std::string message;
};

// 辅助函数：创建默认设置
inline auto create_default_app_settings() -> AppSettings {
  AppSettings settings;
  settings.version = "1.0";
  settings.title = "";

  // 使用新的强类型菜单ID
  settings.app_menu.feature_items = {
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::ScreenshotCapture)), "截图",
       true, 1},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::ScreenshotOpenFolder)),
       "打开相册", true, 2},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::FeatureTogglePreview)),
       "预览窗口", true, 3},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::FeatureToggleOverlay)), "叠加层",
       true, 4},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::FeatureToggleLetterbox)),
       "黑边模式", true, 5},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::WindowResetTransform)),
       "重置窗口", true, 6},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::PanelHide)), "关闭浮窗", true,
       7},
      {std::string(Common::MenuIds::to_string(Common::MenuIds::Id::AppExit)), "退出", false, 8}};

  // 直接访问字段，无需特殊API
  settings.app_menu.aspect_ratios = {
      {"32:9", "32:9", true, 1, false}, {"21:9", "21:9", true, 2, false},
      {"16:9", "16:9", true, 3, false}, {"3:2", "3:2", true, 4, false},
      {"1:1", "1:1", true, 5, false},   {"3:4", "3:4", true, 6, false},
      {"2:3", "2:3", true, 7, false},   {"9:16", "9:16", true, 8, false}};

  // 普通字段直接访问
  settings.app_menu.resolutions = {
      {"Default", "Default", true, 1, false}, {"1080P", "1080P", true, 2, false},
      {"2K", "2K", true, 3, false},           {"4K", "4K", true, 4, false},
      {"6K", "6K", true, 5, false},           {"8K", "8K", true, 6, false},
      {"12K", "12K", true, 7, false}};

  return settings;
}

}  // namespace Features::Settings::Types