module;

export module Core.I18n.Types;

import std;

export namespace Core::I18n::Types {

enum class Language { ZhCN, EnUS };

// App模块相关结构体
struct AppMessages {
  std::string startup;
  std::string startup_suffix;
};

struct AppStrings {
  std::string name;
  AppMessages messages;
};

// Window模块相关结构体
struct WindowMenu {
  std::string select;
  std::string ratio;
  std::string resolution;
  std::string reset;
  std::string toggle_borderless;
};

struct WindowMessages {
  std::string selected;
  std::string adjust_success;
  std::string adjust_failed;
  std::string not_found;
  std::string reset_success;
  std::string reset_failed;
};

struct WindowStrings {
  WindowMenu menu;
  WindowMessages messages;
};

// Features模块相关结构体
struct ScreenshotMenu {
  std::string capture;
  std::string open_folder;
};

struct ScreenshotMessages {
  std::string success;
};

struct ScreenshotStrings {
  ScreenshotMenu menu;
  ScreenshotMessages messages;
};

struct OverlayMenu {
  std::string toggle;
};

struct OverlayMessages {
  std::string conflict;
};

struct OverlayStrings {
  OverlayMenu menu;
  OverlayMessages messages;
};

struct PreviewMenu {
  std::string toggle;
};

struct PreviewMessages {
  std::string conflict;
};

struct PreviewStrings {
  PreviewMenu menu;
  PreviewMessages messages;
};

struct LetterboxMenu {
  std::string toggle;
};

struct LetterboxStrings {
  LetterboxMenu menu;
};

struct FeaturesStrings {
  ScreenshotStrings screenshot;
  OverlayStrings overlay;
  PreviewStrings preview;
  LetterboxStrings letterbox;
};

// Settings模块相关结构体
struct SettingsMenu {
  std::string hotkey;
  std::string config;
  std::string language;
};

struct SettingsMessages {
  std::string hotkey_prompt;
  std::string hotkey_success;
  std::string hotkey_failed;
  std::string hotkey_register_failed;
  std::string config_help;
  std::string load_failed;
  std::string format_error;
  std::string ratio_format_example;
  std::string resolution_format_example;
};

struct SettingsStrings {
  SettingsMenu menu;
  SettingsMessages messages;
};

// UI模块相关结构体
struct TaskbarMenu {
  std::string autohide;
  std::string lower;
};

struct TaskbarStrings {
  TaskbarMenu menu;
};

struct FloatingMenu {
  std::string mode;
  std::string show;
  std::string hide;
};

struct FloatingStrings {
  FloatingMenu menu;
};

struct UiStrings {
  TaskbarStrings taskbar;
  FloatingStrings floating;
};

// I18n模块相关结构体
struct I18nLanguages {
  std::string zh_cn;
  std::string en_us;
};

struct I18nStrings {
  I18nLanguages languages;
};

// System模块相关结构体
struct SystemMenu {
  std::string exit;
  std::string user_guide;
  std::string webview_test;
};

struct SystemMessages {
  std::string feature_not_supported;
};

struct SystemStrings {
  SystemMenu menu;
  SystemMessages messages;
};

// 完整的文本数据结构
struct TextData {
  AppStrings app;
  WindowStrings window;
  FeaturesStrings features;
  SettingsStrings settings;
  UiStrings ui;
  I18nStrings i18n;
  SystemStrings system;
};

}  // namespace Core::I18n::Types