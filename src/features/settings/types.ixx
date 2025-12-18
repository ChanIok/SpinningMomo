module;

export module Features.Settings.Types;

import std;

export namespace Features::Settings::Types {

// 当前设置版本
constexpr int CURRENT_SETTINGS_VERSION = 1;

// 功能项（应用菜单中的功能）
export struct FeatureItem {
  std::string id;       // 如: "screenshot.capture", "screenshot.open_folder"
  bool enabled = true;  // 是否显示在菜单中
  int order = 0;        // 排序序号
};

// 预设项（比例/分辨率）
struct PresetItem {
  std::string id;       // 如: "16:9", "1080P"
  bool enabled = true;  // 是否显示在菜单中
  int order = 0;        // 排序序号
};

// Web 主题设置
export struct WebThemeSettings {
  std::string mode = "system";  // "light" | "dark" | "system"
};

// Web 背景设置
export struct WebBackgroundSettings {
  std::string type = "none";        // "none" | "image"
  std::string image_path = "";
  double opacity = 0.8;             // 0.0 - 1.0
  int blur_amount = 0;              // 0 - 200
};

// 完整的应用设置（重构后的结构）
export struct AppSettings {
  int version = CURRENT_SETTINGS_VERSION;

  // app 分组 - 应用核心设置
  struct App {
    // 快捷键设置
    struct Hotkey {
      struct ToggleVisibility {
        int modifiers = 3;  // Ctrl + Alt
        int key = 82;       // R键
      } toggle_visibility;

      struct Screenshot {
        int modifiers = 0;  // 无修饰键
        int key = 44;       // 印屏键
      } screenshot;
    } hotkey;

    // 语言设置
    struct Language {
      std::string current = "zh-CN";  // zh-CN, en-US
    } language;

    // 日志设置
    struct Logger {
      std::string level = "INFO";  // DEBUG, INFO, ERROR
    } logger;
  } app;

  // window 分组 - 窗口相关设置
  struct Window {
    std::string target_title = "无限暖暖  ";  // 目标窗口标题

    // 任务栏设置
    struct Taskbar {
      bool auto_hide = false;       // 隐藏任务栏
      bool lower_on_resize = true;  // 调整时置底任务栏
    } taskbar;
  } window;

  // features 分组 - 功能特性设置
  struct Features {
    // 截图功能设置
    struct Screenshot {
      std::string screenshot_dir_path = "";  // 游戏相册目录路径
    } screenshot;

    // 黑边模式设置
    struct Letterbox {
      bool enabled = false;  // 是否启用黑边模式
    } letterbox;
  } features;

  // updater 分组 - 更新设置
  struct Updater {
    bool auto_check = true;         // 是否自动检查更新
    int check_interval_hours = 24;  // 检查间隔（小时）

    // 服务器配置
    struct Server {
      std::string name;  // 服务器名称
      std::string url;   // 服务器API地址
    };

    std::vector<Server> servers = {  // 服务器列表（按优先级排序）
        {"GitHub官方", "https://api.github.com/repos/ChanIok/SpinningMomo/releases/latest"}};
  } updater;

  // ui 分组 - UI界面设置
  struct UI {
    // 应用菜单配置
    struct AppMenu {
      std::vector<FeatureItem> feature_items = {{"screenshot.capture", true, 1},
                                                {"screenshot.open_folder", true, 2},
                                                {"feature.toggle_preview", true, 3},
                                                {"feature.toggle_overlay", true, 4},
                                                {"feature.toggle_letterbox", true, 5},
                                                {"feature.toggle_recording", true, 6},
                                                {"window.reset_transform", true, 7},
                                                {"panel.hide", false, 8},
                                                {"app.exit", true, 9}};
      std::vector<PresetItem> aspect_ratios = {
          {"32:9", false, 1}, {"21:9", true, 2}, {"16:9", true, 3}, {"3:2", true, 4},
          {"1:1", true, 5},   {"3:4", true, 6},  {"2:3", true, 7},  {"9:16", true, 8}};
      std::vector<PresetItem> resolutions = {
          {"Default", true, 1}, {"1080P", true, 2}, {"2K", true, 3}, {"4K", true, 4},
          {"6K", true, 5},      {"8K", true, 6},    {"12K", true, 7}};
    } app_menu;

    // AppWindow布局配置
    struct AppWindowLayout {
      int base_item_height = 24;
      int base_title_height = 26;
      int base_separator_height = 1;
      int base_font_size = 12;
      int base_text_padding = 12;
      int base_indicator_width = 3;
      int base_ratio_indicator_width = 4;
      int base_ratio_column_width = 60;
      int base_resolution_column_width = 70;
      int base_settings_column_width = 80;
    } app_window_layout;

    // AppWindow颜色配置
    struct AppWindowColors {
      std::string background = "#282C34A6";
      std::string separator = "#333842A6";
      std::string text = "#DEE8FCFF";
      std::string indicator = "#61AFEFFF";
      std::string hover = "#383D4AA6";
      std::string title_bar = "#282C34A6";
    } app_window_colors;

    // AppWindow主题模式
    std::string app_window_theme_mode = "dark";

    // Web UI 设置
    WebThemeSettings web_theme;
    WebBackgroundSettings background;
  } ui;
};

// 设置变更事件数据
export struct SettingsChangeData {
  AppSettings old_settings;
  AppSettings new_settings;
  std::string change_description;
};

// 获取设置
export struct GetSettingsParams {
  // 空结构体，未来可扩展
};

export using GetSettingsResult = AppSettings;

export using UpdateSettingsParams = AppSettings;

export struct UpdateSettingsResult {
  bool success;
  std::string message;
};

}  // namespace Features::Settings::Types