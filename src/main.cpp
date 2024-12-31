#include "win_config.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <tchar.h>
#include <strsafe.h>
#include <vector>
#include <dwmapi.h>
#include <ShellScalingApi.h>
#include "constants.hpp"
#include "window_utils.hpp"
#include "ui_manager.hpp"

#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Shell32.lib")  // 添加 Shell32.lib 链接器指令

class MenuWindow;  // 前向声明

// 主应用程序类
class SpinningMomoApp {
public:
    // 构造函数和析构函数
    SpinningMomoApp() {
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

    ~SpinningMomoApp() {
        SaveConfig();
    }

    // 初始化相关函数
    bool Initialize(HINSTANCE hInstance) {
        m_hInstance = hInstance;
        if (!RegisterWindowClass(hInstance)) return false;
        if (!CreateAppWindow(hInstance)) return false;
        
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) return false;

        // 创建菜单窗口
        m_menuWindow = std::make_unique<MenuWindow>(hInstance);
        if (!m_menuWindow->Create(m_hwnd, m_ratios, m_resolutions, m_strings,
                                m_currentRatioIndex, m_currentResolutionIndex,
                                m_taskbarAutoHide)) {
            return false;
        }

        // 如果启用了浮动窗口，则默认显示
        if (m_useFloatingWindow && m_menuWindow) {
            m_menuWindow->Show();
        }

        // 注册热键
        if (!RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, m_hotkeyModifiers, m_hotkeyKey)) {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.HOTKEY_REGISTER_FAILED.c_str());
        }

        // 显示启动提示
        std::wstring hotkeyText = GetHotkeyText();
        std::wstring message = m_strings.STARTUP_MESSAGE + hotkeyText + m_strings.STARTUP_MESSAGE_SUFFIX;
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

    // 核心功能函数
    void ShowWindowSelectionMenu() {
        // 更新窗口列表
        m_windows = WindowUtils::GetWindows();
        
        m_trayIcon->ShowContextMenu(
            m_windows,
            m_windowTitle,
            m_ratios,
            m_currentRatioIndex,
            m_resolutions,
            m_currentResolutionIndex,
            m_strings,
            m_topmostEnabled,
            m_taskbarAutoHide,
            m_taskbarLower,
            m_notifyEnabled,
            m_language,
            m_useFloatingWindow,
            m_menuWindow && m_menuWindow->IsVisible()
        );
    }

    void ShowQuickMenu(const POINT& pt) {
        m_trayIcon->ShowQuickMenu(
            pt,
            m_ratios,
            m_currentRatioIndex,
            m_resolutions,
            m_currentResolutionIndex,
            m_strings,
            m_topmostEnabled,
            m_taskbarAutoHide
        );
    }

    void ShowNotification(const TCHAR* title, const TCHAR* message, bool isSuccess = false) {
        // 如果是成功提示，则根据关控制；其他提示始终显示
        if (!isSuccess || m_notifyEnabled) {
            m_trayIcon->ShowBalloon(title, message);
        }
    }

    // 事件处理函数
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
            
            // 更新菜单窗口中的单选按钮状态
            if (m_menuWindow) {
                m_menuWindow->SetCurrentRatio(index);
            }
            
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
            
            // 更新菜单窗口中的单选按钮状态
            if (m_menuWindow) {
                m_menuWindow->SetCurrentResolution(index);
            }
            
            HWND gameWindow = FindTargetWindow();
            if (gameWindow) {
                ApplyWindowTransform(gameWindow);
            } else {
                ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            }
        }
    }

    // 配置相关函数
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
            WritePrivateProfileString(Constants::MENU_SECTION, Constants::MENU_FLOATING, TEXT("1"), m_configPath.c_str());
        }

        // 加载各项配置
        LoadHotkeyConfig();
        LoadWindowConfig();
        LoadNotifyConfig();
        LoadTopmostConfig();
        LoadLanguageConfig();
        LoadTaskbarConfig();
        LoadMenuConfig();
    }

    void SaveConfig() {
        SaveHotkeyConfig();
        SaveWindowConfig();
        SaveNotifyConfig();
        SaveTopmostConfig();
        SaveLanguageConfig();
        SaveTaskbarConfig(); 
        SaveMenuConfig();
    }

    // 工具函数
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

    // 窗口过程函数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        SpinningMomoApp* app = reinterpret_cast<SpinningMomoApp*>(
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
                        case Constants::ID_CONFIG:
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
                        case Constants::ID_AUTOHIDE_TASKBAR:
                            app->ToggleTaskbarAutoHide();
                            break;
                        case Constants::ID_LOWER_TASKBAR:
                            app->ToggleTaskbarLower();
                            break;
                        case Constants::ID_FLOATING_WINDOW:
                            app->ToggleFloatingWindow();
                            break;
                        case Constants::ID_TOGGLE_WINDOW_VISIBILITY:
                            if (app->m_menuWindow) {
                                app->m_menuWindow->ToggleVisibility();
                            }
                            break;
                        case Constants::ID_CAPTURE_WINDOW:
                            app->HandleScreenshot();
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
                    if (app->m_useFloatingWindow) {
                        if (app->m_menuWindow) {
                            app->m_menuWindow->ToggleVisibility();
                        }
                    } else {
                        POINT pt;
                        GetCursorPos(&pt);
                        app->ShowQuickMenu(pt);
                    }
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
    // 窗口和UI相关
    HWND m_hwnd = NULL;                                  // 主窗口句柄
    HINSTANCE m_hInstance = NULL;                        // 应用程序实例句柄
    std::unique_ptr<TrayIcon> m_trayIcon;               // 托盘图标
    std::unique_ptr<MenuWindow> m_menuWindow;           // 菜单窗口
    std::vector<std::pair<HWND, std::wstring>> m_windows;  // 存储窗口列表
    
    // 配置相关
    std::wstring m_configPath;                         // 配置文件完整路径
    std::wstring m_windowTitle;                        // 保存的窗口标题
    UINT m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;   // 热键修饰键
    UINT m_hotkeyKey = 'R';                           // 热键主键
    bool m_hotkeySettingMode = false;                 // 是否处于热键设置模式
    bool m_notifyEnabled = false;                     // 是否显示提示，默认关闭
    bool m_topmostEnabled = false;                    // 是否窗口置顶，默认关闭
    bool m_taskbarAutoHide = false;                   // 任务栏自动隐藏状态
    bool m_taskbarLower = true;                      // 调整时置底任务栏状态
    bool m_useFloatingWindow = true;                 // 是否使用浮动窗口，默认开启
    
    // 窗口变换相关
    std::vector<AspectRatio> m_ratios;                // 预设的宽高比列表
    size_t m_currentRatioIndex = SIZE_MAX;           // 当前选择的比例索引
    bool m_useScreenSize = true;                     // 是否使用屏幕尺寸计算，默认开启
    bool m_windowModified = false;                   // 窗口是否被修改过
    std::vector<ResolutionPreset> m_resolutions;     // 预设的分辨率列表
    size_t m_currentResolutionIndex = 0;             // 当前选择的分辨率索引，默认选择第一个（Default）
    
    // 语言相关
    LocalizedStrings m_strings;                      // 当前语言的字符串
    std::wstring m_language;                         // 当前语言设置

    // 初始化预设的宽高比列表
    void InitializeRatios() {
        m_ratios = {
            {TEXT("32:9"), 32.0/9.0},  // 超宽屏
            {TEXT("21:9"), 21.0/9.0},  // 宽屏
            {TEXT("16:9"), 16.0/9.0},  // 标准宽屏
            {TEXT("3:2"), 3.0/2.0},    // 传统显示器
            {TEXT("1:1"), 1.0},        // 正方形
            {TEXT("2:3"), 2.0/3.0},    // 竖屏
            {TEXT("9:16"), 9.0/16.0}   // 竖屏宽屏
        };
    }

    // 初始化预设的分辨率列表
    void InitializeResolutions() {
        m_resolutions = {
            {TEXT("Default"), 0, 0},         // 默认选项，使用屏幕尺寸计算
            {TEXT("4K"), 3840, 2160},     // 8.3M pixels
            {TEXT("6K"), 5760, 3240},     // 18.7M pixels
            {TEXT("8K"), 7680, 4320},     // 33.2M pixels
            {TEXT("12K"), 11520, 6480}    // 74.6M pixels
        };
    }

    // 从配置文件加载热键设置
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

    // 保存热键设置到配置文件
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

    // 从配置文件加载窗口设置
    void LoadWindowConfig() {
        TCHAR buffer[256];
        if (GetPrivateProfileString(Constants::WINDOW_SECTION,
                                  Constants::WINDOW_TITLE,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_windowTitle = buffer;
        }
    }

    // 保存窗口设置到配置文件
    void SaveWindowConfig() {
        if (!m_windowTitle.empty()) {
            WritePrivateProfileString(Constants::WINDOW_SECTION,
                                    Constants::WINDOW_TITLE,
                                    m_windowTitle.c_str(),
                                    m_configPath.c_str());
        }
    }

    // 从配置文件加载通知设置
    void LoadNotifyConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::NOTIFY_SECTION,
                                  Constants::NOTIFY_ENABLED,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_notifyEnabled = (_wtoi(buffer) != 0);
        }
    }

    // 保存通知设置到配置文件
    void SaveNotifyConfig() {
        WritePrivateProfileString(Constants::NOTIFY_SECTION,
                                Constants::NOTIFY_ENABLED,
                                m_notifyEnabled ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    // 从配置文件加载置顶设置
    void LoadTopmostConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::TOPMOST_SECTION,
                                  Constants::TOPMOST_ENABLED,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_topmostEnabled = (_wtoi(buffer) != 0);
        }
    }

    // 保存置顶设置到配置文件
    void SaveTopmostConfig() {
        WritePrivateProfileString(Constants::TOPMOST_SECTION,
                                Constants::TOPMOST_ENABLED,
                                m_topmostEnabled ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    // 从配置文件加载语言设置
    void LoadLanguageConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::LANG_SECTION,
                                  Constants::LANG_CURRENT,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_language = buffer;
        } else {
            LANGID langId = GetUserDefaultUILanguage();
            WORD primaryLangId = PRIMARYLANGID(langId);
            m_language = (primaryLangId == LANG_CHINESE) ? 
                Constants::LANG_ZH_CN : Constants::LANG_EN_US;
            SaveLanguageConfig();
        }
        m_strings = (m_language == Constants::LANG_EN_US) ? EN_US : ZH_CN;
    }

    // 保存语言设置到配置文件
    void SaveLanguageConfig() {
        WritePrivateProfileString(Constants::LANG_SECTION,
                                Constants::LANG_CURRENT,
                                m_language.c_str(),
                                m_configPath.c_str());
    }

    // 从配置文件加载任务栏设置
    void LoadTaskbarConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::TASKBAR_SECTION,
                                  Constants::TASKBAR_AUTOHIDE,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_taskbarAutoHide = (_wtoi(buffer) != 0);
        }
        
        if (GetPrivateProfileString(Constants::TASKBAR_SECTION,
                                  Constants::TASKBAR_LOWER,
                                  TEXT("1"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_taskbarLower = (_wtoi(buffer) != 0);
        }
    }

    // 保存任务栏设置到配置文件
    void SaveTaskbarConfig() {
        WritePrivateProfileString(Constants::TASKBAR_SECTION,
                                Constants::TASKBAR_AUTOHIDE,
                                m_taskbarAutoHide ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
                                
        WritePrivateProfileString(Constants::TASKBAR_SECTION,
                                Constants::TASKBAR_LOWER,
                                m_taskbarLower ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    // 从配置文件加载菜单设置
    void LoadMenuConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::MENU_SECTION,
                                  Constants::MENU_FLOATING,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_useFloatingWindow = (_wtoi(buffer) != 0);
        }
    }

    // 保存菜单设置到配置文件
    void SaveMenuConfig() {
        WritePrivateProfileString(Constants::MENU_SECTION,
                                Constants::MENU_FLOATING,
                                m_useFloatingWindow ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    // 进入热键设置模式
    void SetHotkey() {
        UnregisterHotKey(m_hwnd, Constants::ID_TRAYICON);
        m_hotkeySettingMode = true;
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.HOTKEY_SETTING.c_str());
    }

    // 切换通知开关状态
    void ToggleNotification() {
        m_notifyEnabled = !m_notifyEnabled;
        SaveNotifyConfig();
    }

    // 切换窗口置顶状态
    void ToggleTopmost() {
        m_topmostEnabled = !m_topmostEnabled;
        SaveTopmostConfig();

        HWND gameWindow = FindTargetWindow();
        if (gameWindow) {
            HWND insertAfter = m_topmostEnabled ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(gameWindow, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    // 切换任务栏自动隐藏状态
    void ToggleTaskbarAutoHide() {
        APPBARDATA abd = {0};
        abd.cbSize = sizeof(APPBARDATA);
        UINT state = (UINT)SHAppBarMessage(ABM_GETSTATE, &abd);
        bool currentAutoHide = (state & ABS_AUTOHIDE) != 0;
        abd.lParam = currentAutoHide ? 0 : ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        m_taskbarAutoHide = !currentAutoHide;
        SaveTaskbarConfig();

        // 更新菜单窗口中的状态
        if (m_menuWindow) {
            m_menuWindow->SetTaskbarAutoHide(m_taskbarAutoHide);
        }
    }

    // 切换界面语言
    void ChangeLanguage(const std::wstring& lang) {
        if (m_language != lang) {
            m_language = lang;
            m_strings = (lang == Constants::LANG_EN_US) ? EN_US : ZH_CN;
            SaveLanguageConfig();
            
            if (m_trayIcon) {
                m_trayIcon->UpdateTip(m_strings.APP_NAME.c_str());
            }
            if (m_menuWindow) {
                m_menuWindow->UpdateMenuItems(m_strings);
            }
        }
    }

    // 重置窗口大小到原始状态
    void ResetWindowSize() {
        HWND gameWindow = FindTargetWindow();
        if (gameWindow) {
            m_currentRatioIndex = SIZE_MAX;
            m_currentResolutionIndex = 0;  // 重置为默认分辨率选项
            
            // 更新菜单窗口中的状态
            if (m_menuWindow) {
                m_menuWindow->SetCurrentRatio(m_currentRatioIndex);
                m_menuWindow->SetCurrentResolution(m_currentResolutionIndex);
            }
            
            if (ApplyWindowTransform(gameWindow)) {
                ShowNotification(m_strings.APP_NAME.c_str(), 
                    m_strings.RESET_SUCCESS.c_str(), true);
            }
        } else {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.WINDOW_NOT_FOUND.c_str());
        }
    }

    // 打开配置文件
    void OpenConfigFile() {
        ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), 
                    m_configPath.c_str(), NULL, SW_SHOW);
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.CONFIG_HELP.c_str());
    }

    // 查找目标窗口
    HWND FindTargetWindow() {
        HWND gameWindow = NULL;
        if (!m_windowTitle.empty()) {
            auto windows = WindowUtils::GetWindows();
            for (const auto& window : windows) {
                if (WindowUtils::CompareWindowTitle(window.second, m_windowTitle)) {
                    gameWindow = window.first;
                    break;
                }
            }
        }
        if (!gameWindow) {
            gameWindow = WindowUtils::FindGameWindow();
        }
        return gameWindow;
    }

    // 应用窗口变换
    bool ApplyWindowTransform(HWND hwnd) {
        if (!hwnd) return false;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        double ratio = static_cast<double>(screenWidth) / screenHeight;

        if (m_currentRatioIndex != SIZE_MAX && m_currentRatioIndex < m_ratios.size()) {
            ratio = m_ratios[m_currentRatioIndex].ratio;
        }

        WindowUtils::Resolution targetRes;
        if (m_currentResolutionIndex != SIZE_MAX && m_currentResolutionIndex < m_resolutions.size()) {
            const auto& preset = m_resolutions[m_currentResolutionIndex];
            if (preset.baseWidth == 0 && preset.baseHeight == 0) {
                // 如果是默认选项，使用屏幕尺寸计算
                targetRes = WindowUtils::CalculateResolutionByScreen(ratio);
            } else {
                targetRes = WindowUtils::CalculateResolution(preset.totalPixels, ratio);
            }
        } else {
            targetRes = WindowUtils::CalculateResolutionByScreen(ratio);
        }

        OutputDebugString((TEXT("Width: ") + std::to_wstring(targetRes.width) + TEXT(", Height: ") + std::to_wstring(targetRes.height) + TEXT("\n")).c_str());
        
        if (WindowUtils::ResizeWindow(hwnd, targetRes.width, targetRes.height, m_topmostEnabled, m_taskbarLower)) {
            m_windowModified = true;
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_SUCCESS.c_str(), true);
            return true;
        } else {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_FAILED.c_str());
            return false;
        }
    }

    // 获取热键文本描述
    std::wstring GetHotkeyText() {
        std::wstring text;
        if (m_hotkeyModifiers & MOD_CONTROL) text += TEXT("Ctrl+");
        if (m_hotkeyModifiers & MOD_ALT) text += TEXT("Alt+");
        if (m_hotkeyModifiers & MOD_SHIFT) text += TEXT("Shift+");
        
        if (m_hotkeyKey >= 'A' && m_hotkeyKey <= 'Z') {
            text += static_cast<TCHAR>(m_hotkeyKey);
        } else {
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

    // 添加自定义宽高比
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

    // 加载自定义宽高比
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
                
                size_t start = 0, end = 0;
                while ((end = ratios.find(TEXT(","), start)) != std::wstring::npos) {
                    std::wstring ratio = ratios.substr(start, end - start);
                    if (!AddCustomRatio(ratio)) {
                        hasError = true;
                        errorDetails += ratio + TEXT(", ");
                    }
                    start = end + 1;
                }
                
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

    // 添加自定义分辨率
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

    // 加载自定义分辨率
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

    // 切换浮动窗口状态
    void ToggleFloatingWindow() {
        bool wasEnabled = m_useFloatingWindow;
        m_useFloatingWindow = !m_useFloatingWindow;
        SaveMenuConfig();

        if (m_menuWindow) {
            if (m_useFloatingWindow && !wasEnabled) {
                // 如果开启浮窗模式，且之前是关闭的，则显示窗口
                m_menuWindow->Show();
            } else if (!m_useFloatingWindow && m_menuWindow->IsVisible()) {
                // 如果关闭浮窗模式，且窗口当前是显示的，则隐藏窗口
                m_menuWindow->Hide();
            }
        }
    }

    // 切换任务栏置底状态
    void ToggleTaskbarLower() {
        m_taskbarLower = !m_taskbarLower;
        SaveTaskbarConfig();
    }

    // 处理截图功能
    void HandleScreenshot() {
        HWND gameWindow = FindTargetWindow();
        if (!gameWindow) {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }

        // 生成文件名（时间戳）
        auto now = std::chrono::system_clock::now();
        std::wstring filename = L"Screenshot_" + 
            std::to_wstring(std::chrono::system_clock::to_time_t(now)) + L".png";
        
        // 保存到ScreenShot文件夹
        std::wstring savePath = WindowUtils::GetScreenshotPath() + L"\\" + filename;
        
        if (WindowUtils::CaptureWindow(gameWindow, savePath)) {
            // 显示成功通知
            ShowNotification(
                m_strings.APP_NAME.c_str(),
                (m_strings.CAPTURE_SUCCESS + savePath).c_str()
            );
        }
    }
};

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow) {
    
    // 设置 DPI 感知
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    SpinningMomoApp app;
    
    if (!app.Initialize(hInstance)) {
        MessageBox(NULL, TEXT("Application initialization failed."), 
                  Constants::APP_NAME, MB_ICONERROR);
        return 1;
    }

    return app.Run();
}

