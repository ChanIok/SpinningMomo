#pragma once
#include "win_config.hpp"
#include <string>

// 常量定义
namespace Constants {
    // 应用程序基本信息
    inline const wchar_t* APP_NAME = L"旋转吧大喵";          
    inline const wchar_t* WINDOW_CLASS = L"SpinningMomoClass";  
    
    // 消息和ID定义
    static const UINT WM_TRAYICON = WM_USER + 1;    // 自定义托盘图标消息
    static const UINT WM_PREVIEW_RCLICK = WM_USER + 2;  // 预览窗口右键点击消息
    static const UINT WM_SHOW_PENDING_NOTIFICATIONS = WM_USER + 100;  // 显示待处理通知消息
    static const UINT WM_SET_HOTKEY_MODE = WM_USER + 101;  // 设置热键模式消息
    constexpr UINT ID_TRAYICON = 1;
    constexpr UINT NOTIFICATION_TIMER_ID = 2;  // 通知显示定时器ID
    
    // 菜单项ID定义
    constexpr UINT ID_WINDOW_BASE = 3000;    // 窗口选择菜单项的基础ID
    constexpr UINT ID_WINDOW_MAX = 3999;     // 窗口选择菜单项的最大ID
    constexpr UINT ID_RATIO_BASE = 4000;     // 比例菜单项的基础ID
    constexpr UINT ID_RESOLUTION_BASE = 5000;  // 分辨率菜单项的基础ID
    
    // 功能菜单项ID
    constexpr UINT ID_ROTATE = 2001;         
    constexpr UINT ID_HOTKEY = 2002;         // 修改热键菜单项ID
    constexpr UINT ID_TASKBAR = 2004;        // 窗口置顶菜单项ID
    constexpr UINT ID_EXIT = 2005;           // 退出菜单项ID
    constexpr UINT ID_RESET = 2006;          // 重置窗口尺寸菜单项ID
    constexpr UINT ID_CONFIG = 2009;         // 打开配置文件菜单项ID
    constexpr UINT ID_AUTOHIDE_TASKBAR = 2007;    // 任务栏自动隐藏菜单项ID
    constexpr UINT ID_LOWER_TASKBAR = 2008;       // 调整时置底任务栏菜单项ID
    constexpr UINT ID_LANG_ZH_CN = 2010;    // 中文选项ID
    constexpr UINT ID_LANG_EN_US = 2011;    // 英文选项ID
    constexpr UINT ID_FLOATING_WINDOW = 2012;    // 浮窗模式选项ID
    constexpr UINT ID_TOGGLE_WINDOW_VISIBILITY = 2013;  // 浮窗显示控制ID
    constexpr UINT ID_CAPTURE_WINDOW = 2014;     // 截图菜单项ID
    constexpr UINT ID_PREVIEW_WINDOW = 2015;     // 预览窗口菜单项ID
    constexpr UINT ID_OPEN_SCREENSHOT = 2016;    // 打开相册菜单项ID
    constexpr UINT ID_USER_GUIDE = 2017;         // 使用指南菜单项ID
    constexpr UINT ID_OVERLAY_WINDOW = 2018;     // 叠加层菜单项ID
    constexpr UINT ID_LETTERBOX_WINDOW = 2019;   // 黑边模式菜单项ID
    
    // 配置文件相关
    inline const wchar_t* CONFIG_FILE = L"config.ini";     // 配置文件名
    
    // 项目文档URL
    inline const wchar_t* DOC_URL_ZH = L"https://chaniok.github.io/SpinningMomo/";
    inline const wchar_t* DOC_URL_EN = L"https://chaniok.github.io/SpinningMomo/en/";
    
    // 配置节和配置项
    inline const wchar_t* WINDOW_SECTION = L"Window";      // 窗口配置
    inline const wchar_t* WINDOW_TITLE = L"Title";         // 窗口标题配置项

    inline const wchar_t* SCREENSHOT_SECTION = L"Screenshot";  // 截图配置节名
    inline const wchar_t* SCREENSHOT_PATH = L"GameAlbumPath"; // 游戏相册路径配置项

    inline const wchar_t* HOTKEY_SECTION = L"Hotkey";      // 热键配置节名
    inline const wchar_t* HOTKEY_MODIFIERS = L"Modifiers"; // 修饰键配置项
    inline const wchar_t* HOTKEY_KEY = L"Key";            // 主键配置项
    
    // 自定义比例和分辨率配置项（现在都在MENU_SECTION中）
    inline const wchar_t* CUSTOM_RATIO_LIST = L"CustomRatioList";       // 自定义比例列表配置项
    inline const wchar_t* CUSTOM_RESOLUTION_LIST = L"CustomResolutionList";      // 自定义分辨率列表配置项
    
    // 语言相关
    inline const wchar_t* LANG_SECTION = L"Language";     // 语言配置节名
    inline const wchar_t* LANG_CURRENT = L"Current";      // 当前语言配置项
    inline const wchar_t* LANG_ZH_CN = L"zh-CN";         // 中文
    inline const wchar_t* LANG_EN_US = L"en-US";         // 英文
    
    inline const wchar_t* TASKBAR_SECTION = L"Taskbar";    // 任务栏配置节名
    inline const wchar_t* TASKBAR_AUTOHIDE = L"AutoHide";  // 任务栏自动隐藏配置项
    inline const wchar_t* TASKBAR_LOWER = L"LowerOnResize"; // 调整时置底任务栏配置项
    
    inline const wchar_t* MENU_SECTION = L"Menu";      // 菜单配置节名
    inline const wchar_t* MENU_FLOATING = L"Floating";  // 浮动窗口配置项
    
    // 菜单项类型字符串
    inline const wchar_t* MENU_ITEM_TYPE_CAPTURE = L"CaptureWindow";
    inline const wchar_t* MENU_ITEM_TYPE_SCREENSHOT = L"OpenScreenshot";
    inline const wchar_t* MENU_ITEM_TYPE_OVERLAY = L"OverlayWindow";
    inline const wchar_t* MENU_ITEM_TYPE_PREVIEW = L"PreviewWindow";
    inline const wchar_t* MENU_ITEM_TYPE_RESET = L"Reset";
    inline const wchar_t* MENU_ITEM_TYPE_CLOSE = L"Close";
    inline const wchar_t* MENU_ITEM_TYPE_EXIT = L"Exit";
    inline const wchar_t* MENU_ITEM_TYPE_LETTERBOX = L"LetterboxWindow";
    
    // 菜单项配置
    inline const wchar_t* MENU_ITEMS = L"MenuItems";  // 菜单项显示配置
    
    // 宽高比和分辨率配置
    inline const wchar_t* ASPECT_RATIO_ITEMS = L"AspectRatioItems";  // 宽高比项目显示配置
    inline const wchar_t* RESOLUTION_ITEMS = L"ResolutionItems";    // 分辨率项目显示配置

    inline const wchar_t* LETTERBOX_SECTION = L"Letterbox";     // 黑边模式配置节名
    inline const wchar_t* LETTERBOX_ENABLED = L"Enabled";       // 黑边模式启用配置项
    
    inline const wchar_t* LOGGER_SECTION = L"Logger";     // 日志配置节名
    inline const wchar_t* LOGGER_LEVEL = L"LogLevel";     // 日志级别配置项
}

// 比例结构体定义
struct AspectRatio {
    std::wstring name;     // 比例名称
    double ratio;          // 宽高比值
    
    AspectRatio(const std::wstring& n, double r) 
        : name(n), ratio(r) {}
};

// 分辨率预设结构体
struct ResolutionPreset {
    std::wstring name;         // 显示名称（如 "4K"）
    UINT64 totalPixels;        // 总像素数
    int baseWidth;            // 基准宽度
    int baseHeight;           // 基准高度
    
    ResolutionPreset(const std::wstring& n, int w, int h) 
        : name(n), totalPixels(static_cast<UINT64>(w) * h), 
          baseWidth(w), baseHeight(h) {}
};

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
};

// 中文字符串
extern const LocalizedStrings ZH_CN;

// 英文字符串
extern const LocalizedStrings EN_US;
