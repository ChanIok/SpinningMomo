#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <tchar.h>
#include <strsafe.h>
#include <vector>
#include <dwmapi.h>
#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")

#define IDI_ICON1 101  // 添加图标ID定义

// 常量定义
namespace Constants {
    // 应用程序基本信息
    const TCHAR* APP_NAME = TEXT("旋转吧大喵");          
    const TCHAR* WINDOW_CLASS = TEXT("SpinningMomoClass");  
    
    // 消息和ID定义
    static const UINT WM_TRAYICON = WM_USER + 1;    // 自定义托盘图标消息
    constexpr UINT ID_TRAYICON = 1;              
    
    // 菜单项ID定义
    constexpr UINT ID_WINDOW_BASE = 3000;    // 窗口选择菜单项的基础ID
    constexpr UINT ID_WINDOW_MAX = 3999;     // 窗口选择菜单项的最大ID
    constexpr UINT ID_RATIO_BASE = 4000;     // 比例菜单项的基础ID
    constexpr UINT ID_RESOLUTION_BASE = 5000;  // 分辨率菜单项的基础ID
    
    // 功能菜单项ID
    constexpr UINT ID_ROTATE = 2001;         
    constexpr UINT ID_HOTKEY = 2002;         // 修改热键菜单项ID
    constexpr UINT ID_NOTIFY = 2003;         // 提示开关菜单项ID
    constexpr UINT ID_TASKBAR = 2004;        // 窗口置顶菜单项ID
    constexpr UINT ID_EXIT = 2005;           // 退出菜单项ID
    constexpr UINT ID_RESET = 2006;          // 重置窗口尺寸菜单项ID
    constexpr UINT ID_CONFIG = 2009;         // 打开配置文件菜单项ID
    
    // 配置文件相关
    const TCHAR* CONFIG_FILE = TEXT("config.ini");     // 配置文件名
    
    // 配置节和配置项
    const TCHAR* WINDOW_SECTION = TEXT("Window");      // 窗口配置
    const TCHAR* WINDOW_TITLE = TEXT("Title");         // 窗口标题配置项

    const TCHAR* HOTKEY_SECTION = TEXT("Hotkey");      // 热键配置节名
    const TCHAR* HOTKEY_MODIFIERS = TEXT("Modifiers"); // 修饰键配置项
    const TCHAR* HOTKEY_KEY = TEXT("Key");            // 主键配置项
    
    const TCHAR* NOTIFY_SECTION = TEXT("Notify");      // 提示配置节名
    const TCHAR* NOTIFY_ENABLED = TEXT("Enabled");     // 提示开关配置项
    
    const TCHAR* TOPMOST_SECTION = TEXT("Topmost");    // 置顶配置节名
    const TCHAR* TOPMOST_ENABLED = TEXT("Enabled");    // 窗口置顶配置项
    
    const TCHAR* CUSTOM_RATIO_SECTION = TEXT("CustomRatio");  // 自定义比例配置节名
    const TCHAR* CUSTOM_RATIO_LIST = TEXT("RatioList");       // 自定义比例列表配置项
    
    const TCHAR* CUSTOM_RESOLUTION_SECTION = TEXT("CustomResolution");  // 自定义分辨率配置节名
    const TCHAR* CUSTOM_RESOLUTION_LIST = TEXT("ResolutionList");      // 自定义分辨率列表配置项
    
    // 语言相关
    const TCHAR* LANG_SECTION = TEXT("Language");     // 语言配置节名
    const TCHAR* LANG_CURRENT = TEXT("Current");      // 当前语言配置项
    const TCHAR* LANG_ZH_CN = TEXT("zh-CN");         // 中文
    const TCHAR* LANG_EN_US = TEXT("en-US");         // 英文
    
    constexpr UINT ID_LANG_ZH_CN = 2010;    // 中文选项ID
    constexpr UINT ID_LANG_EN_US = 2011;    // 英文选项ID
}

// 添加比例结构体定义
struct AspectRatio {
    std::wstring name;     // 比例名称
    double ratio;          // 宽高比值
    
    AspectRatio(const std::wstring& n, double r) 
        : name(n), ratio(r) {}
};

// 添加分辨率预设结构体
struct ResolutionPreset {
    std::wstring name;         // 显示名称（如 "4K"）
    UINT64 totalPixels;        // 总像素数
    int baseWidth;            // 基准宽度
    int baseHeight;           // 基准高度
    
    ResolutionPreset(const std::wstring& n, int w, int h) 
        : name(n), totalPixels(static_cast<UINT64>(w) * h), 
          baseWidth(w), baseHeight(h) {}
};

// 添加字符串资源结构体
struct LocalizedStrings {
    std::wstring APP_NAME;
    std::wstring SELECT_WINDOW;
    std::wstring WINDOW_RATIO;
    std::wstring RESOLUTION;
    std::wstring RESET_WINDOW;
    std::wstring MODIFY_HOTKEY;
    std::wstring SHOW_TIPS;
    std::wstring WINDOW_TOPMOST;
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
    std::wstring LANGUAGE;
    std::wstring CHINESE;
    std::wstring ENGLISH;
    std::wstring HOTKEY_REGISTER_FAILED; 
    std::wstring CONFIG_FORMAT_ERROR;      // 添加格式错误提示
    std::wstring RATIO_FORMAT_EXAMPLE;     // 添加比例格式示例
    std::wstring RESOLUTION_FORMAT_EXAMPLE;  
    std::wstring LOAD_CONFIG_FAILED;       // 添加加载失败提示
};

// 中文字符串
const LocalizedStrings ZH_CN = {
    TEXT("旋转吧大喵"),
    TEXT("选择窗口"),
    TEXT("窗口比例"),
    TEXT("分辨率"),
    TEXT("重置窗口"),
    TEXT("修改热键"),
    TEXT("显示操作提示"),
    TEXT("窗口置顶"),
    TEXT("打开配置文件"),
    TEXT("退出"),
    TEXT("已选择窗口"),
    TEXT("窗口调整成功！"),
    TEXT("窗口调整失败。可能需要管理员权限，或窗口不支持调整大小。"),
    TEXT("未找到目标窗口，请确保窗口已启动。"),
    TEXT("窗口已重置为屏幕大小。"),
    TEXT("重置窗口尺寸失败。"),
    TEXT("请按下新的热键组合...\n支持 Ctrl、Shift、Alt 组合其他按键"),
    TEXT("热键已设置为："),
    TEXT("热键设置失败，已恢复默认热键。"),
    TEXT("配置文件说明：\n1. [CustomRatio] 节用于添加自定义比例\n2. [CustomResolution] 节用于添加自定义分辨率\n3. 保存后重启软件生效"),
    TEXT("窗口比例调整工具已在后台运行。\n按 "),
    TEXT("语言"),
    TEXT("中文"),
    TEXT("English"),
    TEXT("热键注册失败。程序仍可使用，但快捷键将不可用。"),
    TEXT("格式错误："),
    TEXT("请使用正确格式，如：16:10,17:10"),
    TEXT("请使用正确格式，如：3840x2160,7680x4320"),
    TEXT("加载配置失败，请检查配置文件。")
};

// 英文字符串
const LocalizedStrings EN_US = {
    TEXT("SpinningMomo"),
    TEXT("Select Window"),
    TEXT("Window Ratio"),
    TEXT("Resolution"),
    TEXT("Reset Window"),
    TEXT("Modify Hotkey"),
    TEXT("Show Tips"),
    TEXT("Window Topmost"),
    TEXT("Open Config"),
    TEXT("Exit"),
    TEXT("Window Selected"),
    TEXT("Window adjusted successfully!"),
    TEXT("Failed to adjust window. May need administrator privileges, or window doesn't support resizing."),
    TEXT("Target window not found. Please ensure the window is running."),
    TEXT("Window has been reset to screen size."),
    TEXT("Failed to reset window size."),
    TEXT("Please press new hotkey combination...\nSupports Ctrl, Shift, Alt with other keys"),
    TEXT("Hotkey set to: "),
    TEXT("Hotkey setting failed, restored to default."),
    TEXT("Config File Help:\n1. [CustomRatio] section for custom ratios\n2. [CustomResolution] section for custom resolutions\n3. Restart app after saving"),
    TEXT("Window ratio adjustment tool is running in background.\nPress "),
    TEXT("Language"),
    TEXT("中文"),
    TEXT("English"),
    TEXT("Failed to register hotkey. The program can still be used, but the shortcut will not be available."),
    TEXT("Format error: "),
    TEXT("Please use correct format, e.g.: 16:10,17:10"),
    TEXT("Please use correct format, e.g.: 3840x2160,7680x4320"),
    TEXT("Failed to load config, please check the config file.")
};

// 系统托盘图标管理类
class TrayIcon {
public:
    TrayIcon(HWND hwnd) : m_hwnd(hwnd) {
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = hwnd;
        m_nid.uID = Constants::ID_TRAYICON;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = Constants::WM_TRAYICON;
        
        // 使用自定义图标
        HINSTANCE hInstance = GetModuleHandle(NULL);
        m_nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        if (!m_nid.hIcon) {
            // 如果加载失败，使用系统默认图标
            m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }
        
        StringCchCopy(m_nid.szTip, _countof(m_nid.szTip), Constants::APP_NAME);
    }

    ~TrayIcon() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        if (m_nid.hIcon) DestroyIcon(m_nid.hIcon);
    }

    bool Create() {
        return Shell_NotifyIcon(NIM_ADD, &m_nid) != FALSE;
    }

    void ShowBalloon(const TCHAR* title, const TCHAR* message) {
        try {
            m_nid.uFlags = NIF_INFO;
            if (FAILED(StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), title)) ||
                FAILED(StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), message))) {
                // 如果复制失败，使用安全的默认消息
                StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), TEXT("提示"));
                StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), TEXT("发生错误"));
            }
            m_nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
            m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        } catch (...) {
            // 确保即使出错也能恢复正常状态
            m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        }
    }

    void UpdateTip(const TCHAR* tip) {
        StringCchCopy(m_nid.szTip, _countof(m_nid.szTip), tip);
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid = {0};
};

// 窗口调整器类
class WindowResizer {
public:
    // 添加查找指定标题窗口的方法
    static HWND FindGameWindow() {
        // 先尝试查找中文标题
        HWND hwnd = FindWindow(NULL, TEXT("无限暖暖  "));
        if (hwnd) return hwnd;
        
        // 如果找不到中文标题，尝试英文标题
        return FindWindow(NULL, TEXT("Infinity Nikki  "));
    }

    static std::wstring TrimRight(const std::wstring& str) {
        size_t end = str.find_last_not_of(L' ');
        return (end == std::wstring::npos) ? L"" : str.substr(0, end + 1);
    }

    static bool CompareWindowTitle(const std::wstring& title1, const std::wstring& title2) {
        return TrimRight(title1) == TrimRight(title2);
    }

    static bool ResizeWindow(HWND hwnd, int width, int height, bool shouldTopmost) {
        if (!hwnd || !IsWindow(hwnd)) return false;

        // 获取窗口样式
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

        // 计算包含边框的完整窗口大小
        RECT rect = {0, 0, width, height};
        AdjustWindowRectEx(&rect, style, FALSE, exStyle);
        
        // 使用 rect 的 left 和 top 值来调整位置，这些值通常是负数
        int totalWidth = rect.right - rect.left;
        int totalHeight = rect.bottom - rect.top;
        int borderOffsetX = rect.left;  // 左边框的偏移量（负值）
        int borderOffsetY = rect.top;   // 顶部边框的偏移量（负值）

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // 计算屏幕中心位置，考虑边框偏移
        int newLeft = (screenWidth - width) / 2 + borderOffsetX;
        int newTop = (screenHeight - height) / 2 + borderOffsetY;

        // 设置窗口置顶状态
        HWND insertAfter = shouldTopmost ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // 设置新的窗口大小和位置
        return SetWindowPos(hwnd, NULL, newLeft, newTop, totalWidth, totalHeight, 
                          SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    }

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        TCHAR className[256];
        TCHAR windowText[256];
        
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (!GetClassName(hwnd, className, 256)) return TRUE;
        if (!GetWindowText(hwnd, windowText, 256)) return TRUE;

        auto windows = reinterpret_cast<std::vector<std::pair<HWND, std::wstring>>*>(lParam);
        if (windowText[0] != '\0') {  // 只收集有标题的窗口
            windows->push_back({hwnd, windowText});
        }

        return TRUE;
    }

    static std::vector<std::pair<HWND, std::wstring>> GetWindows() {
        std::vector<std::pair<HWND, std::wstring>> windows;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
        return windows;
    }
};

// 主应用程序类
class WindowResizerApp {
public:
    WindowResizerApp() {
        // 获取程序所在目录
        TCHAR exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        
        // 获取程序所在目录
        m_configPath = exePath;
        size_t lastSlash = m_configPath.find_last_of(TEXT("\\"));
        if (lastSlash != std::wstring::npos) {
            m_configPath = m_configPath.substr(0, lastSlash + 1);
        }
        m_configPath += Constants::CONFIG_FILE;

        // 加载配置
        LoadConfig();
        InitializeRatios();
        LoadCustomRatios();
        InitializeResolutions();
        LoadCustomResolutions();
    }

    ~WindowResizerApp() {
        SaveConfig();
    }

    bool Initialize(HINSTANCE hInstance) {
        if (!RegisterWindowClass(hInstance)) return false;
        if (!CreateAppWindow(hInstance)) return false;
        
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) return false;

        // 注册热键
        if (!RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, m_hotkeyModifiers, m_hotkeyKey)) {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.HOTKEY_REGISTER_FAILED.c_str());
        }

        // 显示启动提示
        std::wstring hotkeyText = GetHotkeyText();
        std::wstring message = m_strings.STARTUP_MESSAGE + hotkeyText;
        ShowNotification(m_strings.APP_NAME.c_str(), message.c_str());

        return true;
    }

    int Run() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return (int)msg.wParam;
    }

    void ShowWindowSelectionMenu() {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // 窗口选择
        HMENU hWindowMenu = CreatePopupMenu();
        if (hWindowMenu) {
            auto windows = WindowResizer::GetWindows();
            int id = Constants::ID_WINDOW_BASE;
            for (const auto& window : windows) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (WindowResizer::CompareWindowTitle(window.second, m_windowTitle)) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hWindowMenu, -1, flags, id++, window.second.c_str());
            }

            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                      (UINT_PTR)hWindowMenu, m_strings.SELECT_WINDOW.c_str());
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

            m_windows = std::move(windows);
        }

        // 窗口操作组
        HMENU hRatioMenu = CreatePopupMenu();
        if (hRatioMenu) {
            for (size_t i = 0; i < m_ratios.size(); ++i) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (i == m_currentRatioIndex) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hRatioMenu, -1, flags, Constants::ID_RATIO_BASE + i, m_ratios[i].name.c_str());
            }
            
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                      (UINT_PTR)hRatioMenu, m_strings.WINDOW_RATIO.c_str());
        }

        HMENU hSizeMenu = CreatePopupMenu();
        if (hSizeMenu) {
            for (size_t i = 0; i < m_resolutions.size(); ++i) {
                const auto& preset = m_resolutions[i];
                TCHAR menuText[256];
                _stprintf_s(menuText, _countof(menuText), TEXT("%s (%.1fM)"), 
                    preset.name.c_str(), 
                    preset.totalPixels / 1000000.0);
                
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (i == m_currentResolutionIndex) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hSizeMenu, -1, flags,
                          Constants::ID_RESOLUTION_BASE + i, menuText);
            }
            
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                       (UINT_PTR)hSizeMenu, m_strings.RESOLUTION.c_str());
        }

        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_RESET, m_strings.RESET_WINDOW.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 置顶选项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_topmostEnabled ? MF_CHECKED : 0),
                  Constants::ID_TASKBAR, m_strings.WINDOW_TOPMOST.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 设置组
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_notifyEnabled ? MF_CHECKED : 0), 
                  Constants::ID_NOTIFY, m_strings.SHOW_TIPS.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_HOTKEY, m_strings.MODIFY_HOTKEY.c_str());

        // 语言选择
        HMENU hLangMenu = CreatePopupMenu();
        if (hLangMenu) {
            InsertMenu(hLangMenu, -1, MF_BYPOSITION | MF_STRING | 
                      (m_language == Constants::LANG_ZH_CN ? MF_CHECKED : 0),
                      Constants::ID_LANG_ZH_CN, m_strings.CHINESE.c_str());
            InsertMenu(hLangMenu, -1, MF_BYPOSITION | MF_STRING |
                      (m_language == Constants::LANG_EN_US ? MF_CHECKED : 0),
                      Constants::ID_LANG_EN_US, m_strings.ENGLISH.c_str());

            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                      (UINT_PTR)hLangMenu, m_strings.LANGUAGE.c_str());
        }

        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_CONFIG, m_strings.OPEN_CONFIG.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 退出选项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_EXIT, m_strings.EXIT.c_str());

        // 显示菜单
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hMenu);  // 这会自动销毁所有子菜单
    }

    void HandleWindowSelect(int id) {
        int index = id - Constants::ID_WINDOW_BASE;
        if (index >= 0 && index < m_windows.size()) {
            m_windowTitle = m_windows[index].second;
            SaveConfig();

            ShowNotification(m_strings.APP_NAME.c_str(), 
                    m_strings.WINDOW_SELECTED.c_str());
        }
    }

    void HandleRatioSelect(UINT id) {
        size_t index = id - Constants::ID_RATIO_BASE;
        if (index < m_ratios.size()) {
            m_currentRatioIndex = index;
            
            HWND gameWindow = FindTargetWindow();
            if (gameWindow) {
                ApplyWindowTransform(gameWindow);
            } else {
                ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            }
        }
    }

    void HandleResolutionSelect(UINT id) {
        size_t index = id - Constants::ID_RESOLUTION_BASE;
        if (index < m_resolutions.size()) {
            m_currentResolutionIndex = index;
            
            HWND gameWindow = FindTargetWindow();
            if (gameWindow) {
                ApplyWindowTransform(gameWindow);
            } else {
                ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            }
        }
    }

    void SetHotkey() {
        UnregisterHotKey(m_hwnd, Constants::ID_TRAYICON);
        m_hotkeySettingMode = true;
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.HOTKEY_SETTING.c_str());
    }

    void ShowNotification(const TCHAR* title, const TCHAR* message, bool isSuccess = false) {
        // 如果是成功提示，则根据关控制；其他提示始终显示
        if (!isSuccess || m_notifyEnabled) {
            m_trayIcon->ShowBalloon(title, message);
        }
    }

    void ToggleNotification() {
        m_notifyEnabled = !m_notifyEnabled;
        SaveNotifyConfig();
    }

    void ToggleTopmost() {
        m_topmostEnabled = !m_topmostEnabled;
        SaveTopmostConfig();

        // 立即应用置顶状态到当前窗口
        HWND gameWindow = NULL;
        
        // 查找目标窗口
        if (!m_windowTitle.empty()) {
            auto windows = WindowResizer::GetWindows();
            for (const auto& window : windows) {
                if (WindowResizer::CompareWindowTitle(window.second, m_windowTitle)) {
                    gameWindow = window.first;
                    break;
                }
            }
        }
        
        if (!gameWindow) {
            gameWindow = WindowResizer::FindGameWindow();
        }

        if (gameWindow) {
            // 设置窗口置顶状态
            HWND insertAfter = m_topmostEnabled ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(gameWindow, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    bool IsTopmostEnabled() const {
        return m_topmostEnabled;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowResizerApp* app = reinterpret_cast<WindowResizerApp*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, 
                               reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return 0;
            }

            case WM_KEYDOWN: {
                if (app && app->m_hotkeySettingMode) {
                    // 获取修饰键状态
                    UINT modifiers = 0;
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
                    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
                    if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;

                    // 如果按下了非修饰键
                    if (wParam != VK_CONTROL && wParam != VK_SHIFT && wParam != VK_MENU) {
                        app->m_hotkeyModifiers = modifiers;
                        app->m_hotkeyKey = static_cast<UINT>(wParam);
                        app->m_hotkeySettingMode = false;

                        // 尝试注册新热键
                        if (RegisterHotKey(hwnd, Constants::ID_TRAYICON, modifiers, static_cast<UINT>(wParam))) {
                            std::wstring hotkeyText = app->GetHotkeyText();
                            std::wstring message = app->m_strings.HOTKEY_SET_SUCCESS + hotkeyText;
                            app->ShowNotification(app->m_strings.APP_NAME.c_str(), message.c_str());
                            app->SaveHotkeyConfig();
                        } else {
                            app->m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;
                            app->m_hotkeyKey = 'R';
                            RegisterHotKey(hwnd, Constants::ID_TRAYICON, 
                                        app->m_hotkeyModifiers, app->m_hotkeyKey);
                            app->ShowNotification(app->m_strings.APP_NAME.c_str(), 
                                app->m_strings.HOTKEY_SET_FAILED.c_str());
                            app->SaveHotkeyConfig();
                        }
                    }
                }
                return 0;
            }

            case WM_COMMAND: {
                if (!app) return 0;
                WORD cmd = LOWORD(wParam);
                if (cmd >= Constants::ID_RATIO_BASE && cmd < Constants::ID_RATIO_BASE + 1000) {
                    app->HandleRatioSelect(cmd);
                } else if (cmd >= Constants::ID_RESOLUTION_BASE && cmd < Constants::ID_RESOLUTION_BASE + 1000) {
                    app->HandleResolutionSelect(cmd);
                } else if (cmd >= Constants::ID_WINDOW_BASE && cmd <= Constants::ID_WINDOW_MAX) {
                    app->HandleWindowSelect(cmd);
                } else {
                    switch (cmd) {
                        case Constants::ID_CONFIG:  // 添加：新的配置文件处理
                            app->OpenConfigFile();
                            break;
                        case Constants::ID_HOTKEY:
                            app->SetHotkey();
                            break;
                        case Constants::ID_NOTIFY:
                            app->ToggleNotification();
                            break;
                        case Constants::ID_TASKBAR:
                            app->ToggleTopmost();
                            break;
                        case Constants::ID_EXIT:
                            DestroyWindow(hwnd);
                            break;
                        case Constants::ID_RESET:
                            app->ResetWindowSize();
                            break;
                        case Constants::ID_LANG_ZH_CN:
                            app->ChangeLanguage(Constants::LANG_ZH_CN);
                            break;
                        case Constants::ID_LANG_EN_US:
                            app->ChangeLanguage(Constants::LANG_EN_US);
                            break;
                    }
                }
                return 0;
            }

            case WM_USER + 1: {  // Constants::WM_TRAYICON
                if (app && (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)) {
                    app->ShowWindowSelectionMenu();
                }
                return 0;
            }

            case WM_HOTKEY: {
                if (app && wParam == Constants::ID_TRAYICON) {
                    POINT pt;
                    GetCursorPos(&pt);
                    app->ShowQuickMenu(pt);
                }
                return 0;
            }

            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

private:
    bool RegisterWindowClass(HINSTANCE hInstance) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = Constants::WINDOW_CLASS;
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));        // 添加大图标
        wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));      // 添加小图标
        return RegisterClassEx(&wc) != 0;
    }

    bool CreateAppWindow(HINSTANCE hInstance) {
        m_hwnd = CreateWindow(Constants::WINDOW_CLASS, Constants::APP_NAME,
                            WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstance, this);
        return m_hwnd != NULL;
    }

    HWND m_hwnd = NULL;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::vector<std::pair<HWND, std::wstring>> m_windows;  // 存储窗口列表
    std::wstring m_configPath;                         // 配置文件完整路径
    std::wstring m_windowTitle;                        // 保存的窗口标题
    UINT m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;   // 热键修饰键
    UINT m_hotkeyKey = 'R';                           // 热键主键
    bool m_hotkeySettingMode = false;                 // 是否处于热键设置模式
    bool m_notifyEnabled = false;                     // 是否显示提示，默认关闭
    bool m_topmostEnabled = false;                    // 是否窗口置顶，默认关闭
    std::vector<AspectRatio> m_ratios;
    size_t m_currentRatioIndex = SIZE_MAX;           // 当前选择的比例索引
    bool m_useScreenSize = true;                     // 是否使用屏幕尺寸计算，默认开启
    bool m_windowModified = false;                   // 窗口是否被修改过
    LocalizedStrings m_strings;     // 当前语言的字符串
    std::wstring m_language;        // 当前语言设置
    std::vector<ResolutionPreset> m_resolutions;
    size_t m_currentResolutionIndex = SIZE_MAX;  // 当前选择的分辨率索引，默认不选择

    void InitializeRatios() {
        // 横屏比例（从宽到窄）
        m_ratios.emplace_back(TEXT("32:9"), 32.0/9.0);
        m_ratios.emplace_back(TEXT("21:9"), 21.0/9.0);
        m_ratios.emplace_back(TEXT("16:9"), 16.0/9.0);
        m_ratios.emplace_back(TEXT("3:2"), 3.0/2.0);
        m_ratios.emplace_back(TEXT("1:1"), 1.0/1.0);
        
        // 竖屏比例（从窄到宽）
        m_ratios.emplace_back(TEXT("2:3"), 2.0/3.0);
        m_ratios.emplace_back(TEXT("9:16"), 9.0/16.0);
    }

    void InitializeResolutions() {
        m_resolutions = {
            {L"4K", 3840, 2160},     // 8.3M pixels
            {L"6K", 5760, 3240},     // 18.7M pixels
            {L"8K", 7680, 4320},     // 33.2M pixels
            {L"12K", 11520, 6480}    // 74.6M pixels
        };
    }

    struct Resolution {
        int width;
        int height;
        UINT64 totalPixels;
    };

    Resolution CalculateResolution(const ResolutionPreset& preset, double ratio) {
        UINT64 totalPixels = preset.totalPixels;
        
        // ratio 是宽高比，例如 9/16 = 0.5625
        int width = static_cast<int>(sqrt(totalPixels * ratio));
        int height = static_cast<int>(width / ratio);
        
        // 微调以确保总像素数准确
        if (static_cast<UINT64>(width) * height < totalPixels) {
            width++;
        }
        
        return {width, height, static_cast<UINT64>(width) * height};
    }

    void LoadConfig() {
        // 检查配置文件是否存在
        if (GetFileAttributes(m_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            // 创建默认配置文件
            WritePrivateProfileString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, TEXT(""), m_configPath.c_str());
            WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, TEXT("3"), m_configPath.c_str());
            WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, TEXT("82"), m_configPath.c_str());
            WritePrivateProfileString(Constants::NOTIFY_SECTION, Constants::NOTIFY_ENABLED, TEXT("0"), m_configPath.c_str());
            WritePrivateProfileString(Constants::TOPMOST_SECTION, Constants::TOPMOST_ENABLED, TEXT("0"), m_configPath.c_str());
            WritePrivateProfileString(Constants::CUSTOM_RATIO_SECTION, Constants::CUSTOM_RATIO_LIST, TEXT(""), m_configPath.c_str());
            WritePrivateProfileString(Constants::CUSTOM_RESOLUTION_SECTION, Constants::CUSTOM_RESOLUTION_LIST, TEXT(""), m_configPath.c_str());
        }

        // 加载各项配置
        LoadHotkeyConfig();
        LoadWindowConfig();
        LoadNotifyConfig();
        LoadTopmostConfig();
        LoadLanguageConfig();
    }

    void SaveConfig() {
        SaveHotkeyConfig();
        SaveWindowConfig();
        SaveNotifyConfig();
        SaveTopmostConfig();
        SaveLanguageConfig();
    }

    void LoadHotkeyConfig() {
        TCHAR buffer[32];
        // 读取修饰键
        if (GetPrivateProfileString(Constants::HOTKEY_SECTION, 
                                  Constants::HOTKEY_MODIFIERS,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_hotkeyModifiers = _wtoi(buffer);
        }

        // 读取主键
        if (GetPrivateProfileString(Constants::HOTKEY_SECTION,
                                  Constants::HOTKEY_KEY,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_hotkeyKey = _wtoi(buffer);
        }
    }

    void SaveHotkeyConfig() {
        TCHAR buffer[32];
        // 保存修饰键
        _stprintf_s(buffer, _countof(buffer), TEXT("%u"), m_hotkeyModifiers);
        WritePrivateProfileString(Constants::HOTKEY_SECTION,
                                Constants::HOTKEY_MODIFIERS,
                                buffer, m_configPath.c_str());

        // 保存主键
        _stprintf_s(buffer, _countof(buffer), TEXT("%u"), m_hotkeyKey);
        WritePrivateProfileString(Constants::HOTKEY_SECTION,
                                Constants::HOTKEY_KEY,
                                buffer, m_configPath.c_str());
    }

    void LoadWindowConfig() {
        TCHAR buffer[256];
        if (GetPrivateProfileString(Constants::WINDOW_SECTION,
                                  Constants::WINDOW_TITLE,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_windowTitle = buffer;
        }
    }

    void SaveWindowConfig() {
        if (!m_windowTitle.empty()) {
            WritePrivateProfileString(Constants::WINDOW_SECTION,
                                    Constants::WINDOW_TITLE,
                                    m_windowTitle.c_str(),
                                    m_configPath.c_str());
        }
    }

    void LoadNotifyConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::NOTIFY_SECTION,
                                  Constants::NOTIFY_ENABLED,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_notifyEnabled = (_wtoi(buffer) != 0);
        }
    }

    void SaveNotifyConfig() {
        WritePrivateProfileString(Constants::NOTIFY_SECTION,
                                Constants::NOTIFY_ENABLED,
                                m_notifyEnabled ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    void LoadTopmostConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::TOPMOST_SECTION,
                                  Constants::TOPMOST_ENABLED,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_topmostEnabled = (_wtoi(buffer) != 0);
        }
    }

    void SaveTopmostConfig() {
        WritePrivateProfileString(Constants::TOPMOST_SECTION,
                                Constants::TOPMOST_ENABLED,
                                m_topmostEnabled ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    void LoadLanguageConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::LANG_SECTION,
                                  Constants::LANG_CURRENT,
                                  TEXT(""), buffer, _countof(buffer),  // 注意这里改为空字符串
                                  m_configPath.c_str()) > 0) {
            // 配置文件中有语言设置，直接使用
            m_language = buffer;
        } else {
            // 配置文件中没有语言设置，根据系统语言选择默认值
            LANGID langId = GetUserDefaultUILanguage();
            WORD primaryLangId = PRIMARYLANGID(langId);
            
            // 如果是中文，使用中文，否则使用英文
            m_language = (primaryLangId == LANG_CHINESE) ? 
                Constants::LANG_ZH_CN : Constants::LANG_EN_US;
            
            // 保存默认语言设置
            SaveLanguageConfig();
        }

        // 加载对应语言的字符串
        m_strings = (m_language == Constants::LANG_EN_US) ? EN_US : ZH_CN;
    }

    void SaveLanguageConfig() {
        WritePrivateProfileString(Constants::LANG_SECTION,
                                Constants::LANG_CURRENT,
                                m_language.c_str(),
                                m_configPath.c_str());
    }

    void ChangeLanguage(const std::wstring& lang) {
        if (m_language != lang) {
            m_language = lang;
            m_strings = (lang == Constants::LANG_EN_US) ? EN_US : ZH_CN;
            SaveLanguageConfig();
            
            // 更新托盘图标提示文本
            if (m_trayIcon) {
                m_trayIcon->UpdateTip(m_strings.APP_NAME.c_str());
            }
        }
    }

    void ResetWindowSize() {
        HWND gameWindow = FindTargetWindow();
        if (gameWindow) {
            m_currentRatioIndex = SIZE_MAX;  // 使用屏幕原始比例
            m_currentResolutionIndex = SIZE_MAX; // 使用屏幕原始分辨率
            if (ApplyWindowTransform(gameWindow)) {
                ShowNotification(m_strings.APP_NAME.c_str(), 
                    m_strings.RESET_SUCCESS.c_str(), true);
            }
        } else {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.WINDOW_NOT_FOUND.c_str());
        }
    }

    void ShowQuickMenu(const POINT& pt) {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // 比例选项
        for (size_t i = 0; i < m_ratios.size(); ++i) {
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == m_currentRatioIndex) {
                flags |= MF_CHECKED;
            }
            InsertMenu(hMenu, -1, flags,
                      Constants::ID_RATIO_BASE + i, m_ratios[i].name.c_str());
        }

        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 分辨率选项
        for (size_t i = 0; i < m_resolutions.size(); ++i) {
            const auto& preset = m_resolutions[i];
            TCHAR menuText[256];
            _stprintf_s(menuText, _countof(menuText), TEXT("%s (%.1fM)"), 
                preset.name.c_str(), 
                preset.totalPixels / 1000000.0);
            
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == m_currentResolutionIndex) {
                flags |= MF_CHECKED;
            }
            InsertMenu(hMenu, -1, flags,
                      Constants::ID_RESOLUTION_BASE + i, menuText);
        }

        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 重置和置顶选项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
                  Constants::ID_RESET, m_strings.RESET_WINDOW.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_topmostEnabled ? MF_CHECKED : 0),
                  Constants::ID_TASKBAR, m_strings.WINDOW_TOPMOST.c_str());

        // 显示菜单
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hMenu);
    }

    bool AddCustomRatio(const std::wstring& ratio) {
        size_t colonPos = ratio.find(TEXT(":"));
        if (colonPos == std::wstring::npos) return false;
        
        try {
            double width = std::stod(ratio.substr(0, colonPos));
            double height = std::stod(ratio.substr(colonPos + 1));
            if (height <= 0) return false;
            
            m_ratios.emplace_back(ratio, width/height);
            return true;
        } catch (...) {
            return false;
        }
    }

    void LoadCustomRatios() {
        try {
            TCHAR buffer[1024];
            if (GetPrivateProfileString(Constants::CUSTOM_RATIO_SECTION,
                                      Constants::CUSTOM_RATIO_LIST,
                                      TEXT(""), buffer, _countof(buffer),
                                      m_configPath.c_str()) > 0) {
                std::wstring ratios = buffer;
                if (ratios.empty()) return;

                bool hasError = false;
                std::wstring errorDetails;
                
                // 分割并处理每个比例
                size_t start = 0, end = 0;
                while ((end = ratios.find(TEXT(","), start)) != std::wstring::npos) {
                    std::wstring ratio = ratios.substr(start, end - start);
                    if (!AddCustomRatio(ratio)) {
                        hasError = true;
                        errorDetails += ratio + TEXT(", ");
                    }
                    start = end + 1;
                }
                
                // 处理最后一个比例
                if (start < ratios.length()) {
                    std::wstring ratio = ratios.substr(start);
                    if (!AddCustomRatio(ratio)) {
                        hasError = true;
                        errorDetails += ratio;
                    }
                }

                if (hasError) {
                    std::wstring errorMsg = m_strings.CONFIG_FORMAT_ERROR + errorDetails + 
                                          TEXT("\n") + m_strings.RATIO_FORMAT_EXAMPLE;
                    MessageBox(NULL, errorMsg.c_str(), m_strings.APP_NAME.c_str(), 
                              MB_ICONWARNING | MB_OK);
                }
            }
        } catch (...) {
            MessageBox(NULL, m_strings.LOAD_CONFIG_FAILED.c_str(), 
                m_strings.APP_NAME.c_str(), MB_ICONERROR | MB_OK);
        }
    }
    
    bool AddCustomResolution(const std::wstring& resolution) {
        try {
            size_t xPos = resolution.find(TEXT("x"));
            if (xPos == std::wstring::npos) return false;

            int width = std::stoi(resolution.substr(0, xPos));
            int height = std::stoi(resolution.substr(xPos + 1));

            if (width <= 0 || height <= 0) return false;

            std::wstring name = resolution;
            m_resolutions.emplace_back(name, width, height);
            return true;
        } catch (...) {
            return false;
        }
    }

    void LoadCustomResolutions() {
        TCHAR buffer[1024];
        if (GetPrivateProfileString(Constants::CUSTOM_RESOLUTION_SECTION,
                                  Constants::CUSTOM_RESOLUTION_LIST,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            std::wstring resolutions = buffer;
            if (resolutions.empty()) return;

            bool hasError = false;
            std::wstring errorDetails;

            size_t start = 0, end = 0;
            while ((end = resolutions.find(TEXT(","), start)) != std::wstring::npos) {
                if (!AddCustomResolution(resolutions.substr(start, end - start))) {
                    hasError = true;
                    errorDetails += resolutions.substr(start, end - start) + TEXT(", ");
                }
                start = end + 1;
            }
            
            if (start < resolutions.length()) {
                if (!AddCustomResolution(resolutions.substr(start))) {
                    hasError = true;
                    errorDetails += resolutions.substr(start);
                }
            }

            if (hasError) {
                std::wstring errorMsg = m_strings.CONFIG_FORMAT_ERROR + errorDetails + 
                                      TEXT("\n") + m_strings.RESOLUTION_FORMAT_EXAMPLE;
                MessageBox(NULL, errorMsg.c_str(), m_strings.APP_NAME.c_str(), 
                          MB_ICONWARNING | MB_OK);
            }
        }
    }

    std::wstring GetHotkeyText() {
        std::wstring text;
        
        // 添加修饰键
        if (m_hotkeyModifiers & MOD_CONTROL) text += TEXT("Ctrl+");
        if (m_hotkeyModifiers & MOD_ALT) text += TEXT("Alt+");
        if (m_hotkeyModifiers & MOD_SHIFT) text += TEXT("Shift+");
        
        // 添加主键
        if (m_hotkeyKey >= 'A' && m_hotkeyKey <= 'Z') {
            text += static_cast<TCHAR>(m_hotkeyKey);
        } else {
            // 处理特殊键
            switch (m_hotkeyKey) {
                case VK_F1: text += TEXT("F1"); break;
                case VK_F2: text += TEXT("F2"); break;
                case VK_F3: text += TEXT("F3"); break;
                case VK_F4: text += TEXT("F4"); break;
                case VK_F5: text += TEXT("F5"); break;
                case VK_F6: text += TEXT("F6"); break;
                case VK_F7: text += TEXT("F7"); break;
                case VK_F8: text += TEXT("F8"); break;
                case VK_F9: text += TEXT("F9"); break;
                case VK_F10: text += TEXT("F10"); break;
                case VK_F11: text += TEXT("F11"); break;
                case VK_F12: text += TEXT("F12"); break;
                case VK_NUMPAD0: text += TEXT("Num0"); break;
                case VK_NUMPAD1: text += TEXT("Num1"); break;
                case VK_NUMPAD2: text += TEXT("Num2"); break;
                case VK_NUMPAD3: text += TEXT("Num3"); break;
                case VK_NUMPAD4: text += TEXT("Num4"); break;
                case VK_NUMPAD5: text += TEXT("Num5"); break;
                case VK_NUMPAD6: text += TEXT("Num6"); break;
                case VK_NUMPAD7: text += TEXT("Num7"); break;
                case VK_NUMPAD8: text += TEXT("Num8"); break;
                case VK_NUMPAD9: text += TEXT("Num9"); break;
                case VK_MULTIPLY: text += TEXT("Num*"); break;
                case VK_ADD: text += TEXT("Num+"); break;
                case VK_SUBTRACT: text += TEXT("Num-"); break;
                case VK_DECIMAL: text += TEXT("Num."); break;
                case VK_DIVIDE: text += TEXT("Num/"); break;
                default: text += static_cast<TCHAR>(m_hotkeyKey); break;
            }
        }
        
        return text;
    }

    bool ApplyWindowTransform(HWND hwnd) {
        if (!hwnd) return false;

        // 获取当前选择的比例
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        double ratio = static_cast<double>(screenWidth) / screenHeight;  // 默认使用屏幕比例
        
        if (m_currentRatioIndex != SIZE_MAX && m_currentRatioIndex < m_ratios.size()) {
            ratio = m_ratios[m_currentRatioIndex].ratio;
        }

        // 计算目标分辨率
        Resolution targetRes;
        if (m_currentResolutionIndex != SIZE_MAX && m_currentResolutionIndex < m_resolutions.size()) {
            // 使用预设分辨率
            const auto& preset = m_resolutions[m_currentResolutionIndex];
            targetRes = CalculateResolution(preset, ratio);
        } else {
            // 使用新的计算方法，确保不超出屏幕范围
            
            // 方案1：使用屏幕宽度计算高度
            int height1 = static_cast<int>(screenWidth / ratio);
            
            // 方案2：使用屏幕高度计算宽度
            int width2 = static_cast<int>(screenHeight * ratio);
            
            // 选择不超出屏幕的方案
            if (width2 <= screenWidth) {
                // 如果基于高度计算的宽度不超出屏幕，使用方案2
                targetRes = {width2, screenHeight, static_cast<UINT64>(width2) * screenHeight};
            } else {
                // 否则使用方案1
                targetRes = {screenWidth, height1, static_cast<UINT64>(screenWidth) * height1};
            }
        }

        // 调整窗口大小
        if (WindowResizer::ResizeWindow(hwnd, targetRes.width, targetRes.height, m_topmostEnabled)) {
            m_windowModified = true;
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_SUCCESS.c_str(), true);
            return true;
        } else {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_FAILED.c_str());
            return false;
        }
    }

    HWND FindTargetWindow() {
        HWND gameWindow = NULL;
        
        // 查找目标窗口
        if (!m_windowTitle.empty()) {
            auto windows = WindowResizer::GetWindows();
            for (const auto& window : windows) {
                if (WindowResizer::CompareWindowTitle(window.second, m_windowTitle)) {
                    gameWindow = window.first;
                    break;
                }
            }
        }
        
        if (!gameWindow) {
            gameWindow = WindowResizer::FindGameWindow();
        }

        return gameWindow;
    }

    void OpenConfigFile() {
        ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), 
                    m_configPath.c_str(), NULL, SW_SHOW);
        
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.CONFIG_HELP.c_str());
    }
};

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow) {
    
    // 设置 DPI 感知
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    WindowResizerApp app;
    
    if (!app.Initialize(hInstance)) {
        MessageBox(NULL, TEXT("Application initialization failed."), 
                  Constants::APP_NAME, MB_ICONERROR);
        return 1;
    }

    return app.Run();
}