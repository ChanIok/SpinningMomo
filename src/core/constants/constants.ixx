module;

#include <windows.h>

#include <iostream>

export module Core.Constants;

import std;

// 常量定义 - 保持与原有constants.hpp完全相同的接口
export namespace Core::Constants {

// 应用程序基本信息
const std::wstring APP_NAME = L"SpinningMomo";
const std::wstring WINDOW_CLASS = L"SpinningMomoClass";

// 消息和ID定义
constexpr UINT WM_TRAYICON = WM_USER + 1;                      // 自定义托盘图标消息
constexpr UINT WM_PREVIEW_RCLICK = WM_USER + 2;                // 预览窗口右键点击消息
constexpr UINT WM_SHOW_PENDING_NOTIFICATIONS = WM_USER + 100;  // 显示待处理通知消息
constexpr UINT WM_SET_HOTKEY_MODE = WM_USER + 101;             // 设置热键模式消息
constexpr UINT HOTKEY_ID = 1;                                  // 热键ID
constexpr UINT NOTIFICATION_TIMER_ID = 2;                      // 通知显示定时器ID

// 菜单项ID定义
constexpr UINT ID_WINDOW_BASE = 3000;      // 窗口选择菜单项的基础ID
constexpr UINT ID_WINDOW_MAX = 3999;       // 窗口选择菜单项的最大ID
constexpr UINT ID_RATIO_BASE = 4000;       // 比例菜单项的基础ID
constexpr UINT ID_RESOLUTION_BASE = 5000;  // 分辨率菜单项的基础ID

// 功能菜单项ID
constexpr UINT ID_ROTATE = 2001;
constexpr UINT ID_HOTKEY = 2002;                    // 修改热键菜单项ID
constexpr UINT ID_TASKBAR = 2004;                   // 窗口置顶菜单项ID
constexpr UINT ID_EXIT = 2005;                      // 退出菜单项ID
constexpr UINT ID_RESET = 2006;                     // 重置窗口尺寸菜单项ID
constexpr UINT ID_CONFIG = 2009;                    // 打开配置文件菜单项ID
constexpr UINT ID_AUTOHIDE_TASKBAR = 2007;          // 任务栏自动隐藏菜单项ID
constexpr UINT ID_LOWER_TASKBAR = 2008;             // 调整时置底任务栏菜单项ID
constexpr UINT ID_LANG_ZH_CN = 2010;                // 中文选项ID
constexpr UINT ID_LANG_EN_US = 2011;                // 英文选项ID
constexpr UINT ID_FLOATING_WINDOW = 2012;           // 浮窗模式选项ID
constexpr UINT ID_TOGGLE_WINDOW_VISIBILITY = 2013;  // 浮窗显示控制ID
constexpr UINT ID_CAPTURE_WINDOW = 2014;            // 截图菜单项ID
constexpr UINT ID_PREVIEW_WINDOW = 2015;            // 预览窗口菜单项ID
constexpr UINT ID_OPEN_SCREENSHOT = 2016;           // 打开相册菜单项ID
constexpr UINT ID_USER_GUIDE = 2017;                // 使用指南菜单项ID
constexpr UINT ID_OVERLAY_WINDOW = 2018;            // 叠加层菜单项ID
constexpr UINT ID_LETTERBOX_WINDOW = 2019;          // 黑边模式菜单项ID
constexpr UINT ID_TOGGLE_BORDERLESS = 2020;         // 切换边框模式菜单项ID
constexpr UINT ID_WEBVIEW_TEST = 2021;              // WebView测试菜单项ID

// 资源 ID
constexpr int IDI_ICON1 = 101;

// 配置文件相关
const std::wstring CONFIG_FILE = L"config.ini";  // 配置文件名

// 项目文档URL
const std::wstring DOC_URL_ZH = L"https://chaniok.github.io/SpinningMomo/";
const std::wstring DOC_URL_EN = L"https://chaniok.github.io/SpinningMomo/en/";

// 配置节和配置项
const std::wstring WINDOW_SECTION = L"Window";  // 窗口配置
const std::wstring WINDOW_TITLE = L"Title";     // 窗口标题配置项

const std::wstring SCREENSHOT_SECTION = L"Screenshot";  // 截图配置节名
const std::wstring SCREENSHOT_PATH = L"GameAlbumPath";  // 游戏相册路径配置项

const std::wstring HOTKEY_SECTION = L"Hotkey";       // 热键配置节名
const std::wstring HOTKEY_MODIFIERS = L"Modifiers";  // 修饰键配置项
const std::wstring HOTKEY_KEY = L"Key";              // 主键配置项

// 自定义比例和分辨率配置项
const std::wstring CUSTOM_RATIO_LIST = L"CustomRatioList";            // 自定义比例列表配置项
const std::wstring CUSTOM_RESOLUTION_LIST = L"CustomResolutionList";  // 自定义分辨率列表配置项

// 语言相关
const std::wstring LANG_SECTION = L"Language";  // 语言配置节名
const std::wstring LANG_CURRENT = L"Current";   // 当前语言配置项
const std::wstring LANG_ZH_CN = L"zh-CN";       // 中文
const std::wstring LANG_EN_US = L"en-US";       // 英文

const std::wstring TASKBAR_SECTION = L"Taskbar";      // 任务栏配置节名
const std::wstring TASKBAR_AUTOHIDE = L"AutoHide";    // 任务栏自动隐藏配置项
const std::wstring TASKBAR_LOWER = L"LowerOnResize";  // 调整时置底任务栏配置项

const std::wstring MENU_SECTION = L"Menu";       // 菜单配置节名
const std::wstring MENU_FLOATING = L"Floating";  // 浮动窗口配置项

// 菜单项配置
const std::wstring MENU_ITEMS = L"MenuItems";  // 菜单项显示配置

// 宽高比和分辨率配置
const std::wstring ASPECT_RATIO_ITEMS = L"AspectRatioItems";  // 宽高比项目显示配置
const std::wstring RESOLUTION_ITEMS = L"ResolutionItems";     // 分辨率项目显示配置

const std::wstring LETTERBOX_SECTION = L"Letterbox";  // 黑边模式配置节名
const std::wstring LETTERBOX_ENABLED = L"Enabled";    // 黑边模式启用配置项

const std::wstring LOGGER_SECTION = L"Logger";  // 日志配置节名
const std::wstring LOGGER_LEVEL = L"LogLevel";  // 日志级别配置项

// 字符串资源结构体
struct LocalizedStrings {
  std::wstring APP_NAME;
  std::wstring SELECT_WINDOW;
  std::wstring WINDOW_RATIO;
  std::wstring RESOLUTION;
  std::wstring RESET_WINDOW;
  std::wstring MODIFY_HOTKEY;
  std::wstring OPEN_CONFIG;
  std::wstring EXIT;
  std::wstring WINDOW_SELECTED;
  std::wstring ADJUST_SUCCESS;
  std::wstring ADJUST_FAILED;
  std::wstring WINDOW_NOT_FOUND;
  std::wstring RESET_SUCCESS;
  std::wstring RESET_FAILED;
  std::wstring HOTKEY_SETTING;
  std::wstring HOTKEY_SET_SUCCESS;
  std::wstring HOTKEY_SET_FAILED;
  std::wstring CONFIG_HELP;
  std::wstring STARTUP_MESSAGE;
  std::wstring STARTUP_MESSAGE_SUFFIX;
  std::wstring LANGUAGE;
  std::wstring CHINESE;
  std::wstring ENGLISH;
  std::wstring HOTKEY_REGISTER_FAILED;
  std::wstring CONFIG_FORMAT_ERROR;
  std::wstring RATIO_FORMAT_EXAMPLE;
  std::wstring RESOLUTION_FORMAT_EXAMPLE;
  std::wstring LOAD_CONFIG_FAILED;
  std::wstring TASKBAR_AUTOHIDE;
  std::wstring TASKBAR_LOWER;
  std::wstring FLOATING_MODE;
  std::wstring SHOW_WINDOW;
  std::wstring CLOSE_WINDOW;
  std::wstring CAPTURE_WINDOW;
  std::wstring CAPTURE_SUCCESS;
  std::wstring PREVIEW_WINDOW;
  std::wstring OPEN_SCREENSHOT;
  std::wstring USER_GUIDE;
  std::wstring FEATURE_NOT_SUPPORTED;
  std::wstring OVERLAY_WINDOW;
  std::wstring FEATURE_CONFLICT;
  std::wstring LETTERBOX_WINDOW;
  std::wstring TOGGLE_BORDERLESS;
  std::wstring WEBVIEW_TEST;
};

// 中文字符串声明
extern const LocalizedStrings ZH_CN;

// 英文字符串声明
extern const LocalizedStrings EN_US;

}  // namespace Core::Constants