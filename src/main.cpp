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
#include "tray_icon.hpp"
#include "menu_window.hpp"
#include "preview_window.hpp"
#include "notification_manager.hpp"
#include "config_manager.hpp"
#include "overlay_window.hpp"
#include "letterbox_window.hpp"
#include "logger.hpp"
#include "event_handler.hpp"

// 主应用程序类
class SpinningMomoApp {
public:
    SpinningMomoApp() = default;
    ~SpinningMomoApp() {
        UnregisterHotKey(m_hwnd, 1);
    }

    bool Initialize(HINSTANCE hInstance) {
        Logger::GetInstance().Initialize();
        LOG_INFO("Program initialization started");

        // 记录系统信息
        LogSystemInfo();

        // 初始化配置管理器
        m_configManager = std::make_unique<ConfigManager>();
        m_configManager->Initialize();
        m_configManager->LoadAllConfigs();
        
        // 从配置获取日志级别
        LogLevel configLogLevel = m_configManager->GetLogLevel();
        
        #ifndef NDEBUG
        // Debug版本下始终使用DEBUG级别
        Logger::GetInstance().SetLogLevel(LogLevel::DEBUG);
        LOG_INFO("Debug build: Setting log level to DEBUG");
        #else
            // Release版本使用配置文件中的设置
            Logger::GetInstance().SetLogLevel(configLogLevel);
            LOG_INFO("Log level set to %s", Logger::GetInstance().GetLevelString(configLogLevel));
        #endif

        // 创建通知管理器
        m_notificationManager = std::make_unique<NotificationManager>(hInstance);

        // 初始化字符串
        m_language = m_configManager->GetLanguage();
        m_strings = (m_language == Constants::LANG_EN_US) ? EN_US : ZH_CN;

        // 检查屏幕捕获功能是否可用
        m_isScreenCaptureSupported = WindowUtils::IsWindowsCaptureSupported();
        LOG_INFO("Screen capture feature is %s", m_isScreenCaptureSupported ? "available" : "not available");

        // 获取宽高比和分辨率列表
        ConfigLoadResult ratioResult = m_configManager->GetAspectRatios(m_strings);
        if (!ratioResult.success) {
            LOG_ERROR("Failed to load aspect ratio configuration: %s", ratioResult.errorDetails);
            AddPendingNotification(m_strings.APP_NAME.c_str(), ratioResult.errorDetails.c_str());
        }
        m_ratios = std::move(ratioResult.ratios);
        
        ConfigLoadResult resolutionResult = m_configManager->GetResolutionPresets(m_strings);
        if (!resolutionResult.success) {
            LOG_ERROR("Failed to load resolution configuration: %s", resolutionResult.errorDetails);
            AddPendingNotification(m_strings.APP_NAME.c_str(), resolutionResult.errorDetails.c_str());
        }
        m_resolutions = std::move(resolutionResult.resolutions);

        // 初始化UI组件
        if (!RegisterWindowClass(hInstance)) {
            LOG_ERROR("Failed to register window class");
            return false;
        }
        if (!CreateAppWindow(hInstance)) {
            LOG_ERROR("Failed to create application window");
            return false;
        }

        // 创建托盘图标
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) {
            LOG_ERROR("Failed to create tray icon");
            return false;
        }

        // 创建预览窗口
        m_previewWindow = std::make_unique<PreviewWindow>();
        if (!m_previewWindow->Initialize(hInstance, m_hwnd)) {
            LOG_ERROR("Failed to initialize preview window");
            return false;
        }

        // 创建叠加层窗口
        m_overlayWindow = std::make_unique<OverlayWindow>();
        if (!m_overlayWindow->Initialize(hInstance, m_hwnd)) {
            LOG_ERROR("Failed to initialize overlay window");
            return false;
        }
        
        // 创建黑边窗口
        m_letterboxWindow = std::make_unique<LetterboxWindow>();
        if (!m_letterboxWindow->Initialize(hInstance)) {
            LOG_ERROR("Failed to initialize letterbox window");
            return false;
        }

        // 创建菜单窗口
        m_menuWindow = std::make_unique<MenuWindow>(hInstance);
        // 设置菜单项显示配置
        m_menuWindow->SetMenuItemsToShow(m_configManager->GetMenuItemsToShow());
        // 从配置中加载黑边模式状态
        m_isLetterboxEnabled = m_configManager->GetLetterboxEnabled();
        m_overlayWindow->SetLetterboxMode(m_isLetterboxEnabled);
        
        if (!m_menuWindow->Create(m_hwnd, m_ratios, m_resolutions, m_strings,
                            m_currentRatioIndex, m_currentResolutionIndex,
                            m_isPreviewEnabled, m_isOverlayEnabled, m_isLetterboxEnabled)) {
            LOG_ERROR("Failed to create menu window");
            return false;
        }

        // 创建事件处理器
        m_eventHandler = std::make_unique<EventHandler>(
            m_hwnd,
            m_configManager.get(),
            m_notificationManager.get(),
            m_strings,
            m_menuWindow.get(),
            m_previewWindow.get(),
            m_overlayWindow.get(),
            m_letterboxWindow.get(),
            m_trayIcon.get(),
            m_isPreviewEnabled,
            m_isOverlayEnabled,
            m_isLetterboxEnabled,
            m_currentRatioIndex,
            m_currentResolutionIndex,
            m_ratios,
            m_resolutions,
            m_language,
            m_windows,
            m_isScreenCaptureSupported
        );

        // 如果启用了浮动窗口，则默认显示
        if (m_configManager->GetUseFloatingWindow() && m_menuWindow) {
            m_menuWindow->Show();
        }

        // 注册热键
        bool hotkeyRegistered = RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, 
                          m_configManager->GetHotkeyModifiers(), 
                          m_configManager->GetHotkeyKey());
        
        if (!hotkeyRegistered) {
            LOG_ERROR("Failed to register global hotkey");
            AddPendingNotification(m_strings.APP_NAME.c_str(), 
                m_strings.HOTKEY_REGISTER_FAILED.c_str());
        } else {
            // 只在热键注册成功时显示启动提示
            std::wstring hotkeyText = m_eventHandler->GetHotkeyText();
            std::wstring message = m_strings.STARTUP_MESSAGE + hotkeyText + m_strings.STARTUP_MESSAGE_SUFFIX;
            AddPendingNotification(m_strings.APP_NAME.c_str(), message.c_str());
        }
        LOG_INFO("Program initialization completed successfully");
        return true;
    }

    int Run() {
        m_messageLoopStarted = true;  // 设置消息循环开始标记
        PostMessage(m_hwnd, Constants::WM_SHOW_PENDING_NOTIFICATIONS, 0, 0);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return (int)msg.wParam;
    }

private:
    // 窗口和UI组件
    HWND m_hwnd = NULL;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::unique_ptr<MenuWindow> m_menuWindow;
    std::unique_ptr<PreviewWindow> m_previewWindow;
    std::unique_ptr<NotificationManager> m_notificationManager;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<OverlayWindow> m_overlayWindow;
    std::unique_ptr<LetterboxWindow> m_letterboxWindow;
    std::unique_ptr<EventHandler> m_eventHandler; // 新增事件处理器

    // 应用状态
    bool m_isPreviewEnabled = false;
    bool m_hotkeySettingMode = false;
    bool m_messageLoopStarted = false;
    bool m_isScreenCaptureSupported = false;
    bool m_isOverlayEnabled = false;
    bool m_isLetterboxEnabled = false;
    size_t m_currentRatioIndex = SIZE_MAX;
    size_t m_currentResolutionIndex = 0;
    std::vector<std::pair<HWND, std::wstring>> m_windows;
    
    // 预设数据
    std::vector<AspectRatio> m_ratios;
    std::vector<ResolutionPreset> m_resolutions;
    
    // 界面文本
    LocalizedStrings m_strings;
    std::wstring m_language;

    // 记录最后一次托盘图标点击时间
    ULONGLONG m_lastTrayClickTime = 0;

    // 待显示通知队列
    struct PendingNotification {
        std::wstring title;
        std::wstring message;
        NotificationWindow::NotificationType type;
    };
    std::vector<PendingNotification> m_pendingNotifications;
    size_t m_currentNotificationIndex = 0;

    // 添加待显示通知
    void AddPendingNotification(const TCHAR* title, const TCHAR* message, bool isError = false) {
        m_pendingNotifications.push_back({
            title,
            message,
            isError ? NotificationWindow::NotificationType::Error 
                   : NotificationWindow::NotificationType::Info
        });
    }

    bool RegisterWindowClass(HINSTANCE hInstance) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = Constants::WINDOW_CLASS;
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        return RegisterClassEx(&wc) != 0;
    }

    bool CreateAppWindow(HINSTANCE hInstance) {
        m_hwnd = CreateWindow(Constants::WINDOW_CLASS, Constants::APP_NAME,
                            WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstance, this);
        return m_hwnd != NULL;
    }

    void LogSystemInfo() {
        OSVERSIONINFOEXW osvi = {0};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
        
        // 使用RtlGetVersion
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll) {
            typedef NTSTATUS(WINAPI* RtlGetVersionFunc)(OSVERSIONINFOEXW*);
            RtlGetVersionFunc RtlGetVersion = (RtlGetVersionFunc)GetProcAddress(ntdll, "RtlGetVersion");
            if (RtlGetVersion) {
                RtlGetVersion(&osvi);
                const char* windowsName = "Windows";
                if (osvi.dwMajorVersion == 10) {
                    if (osvi.dwBuildNumber >= 22000) {
                        windowsName = "Windows 11";
                    } else {
                        windowsName = "Windows 10";
                    }
                } else {
                    windowsName = "Windows";
                }
                
                LOG_INFO("%s Version: %d.%d.%d",
                    windowsName, osvi.dwMajorVersion, 
                    osvi.dwMinorVersion, osvi.dwBuildNumber);
            }
        }
    
        // 获取显示器信息
        int monitorCount = GetSystemMetrics(SM_CMONITORS);
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        LOG_INFO("Display: Monitors: %d, Primary screen: %dx%d", 
            monitorCount, screenWidth, screenHeight);
    
        // 获取DPI设置
        HDC hdc = GetDC(NULL);
        if (hdc) {
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            LOG_INFO("DPI Settings: %dx%d", dpiX, dpiY);
            ReleaseDC(NULL, hdc);
        }
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
                        app->m_hotkeySettingMode = false;

                        // 尝试注册新热键
                        if (RegisterHotKey(hwnd, Constants::ID_TRAYICON, modifiers, static_cast<UINT>(wParam))) {
                            app->m_configManager->SetHotkeyModifiers(modifiers);
                            app->m_configManager->SetHotkeyKey(static_cast<UINT>(wParam));
                            app->m_configManager->SaveHotkeyConfig();
                            
                            std::wstring hotkeyText = app->m_eventHandler->GetHotkeyText();
                            std::wstring message = app->m_strings.HOTKEY_SET_SUCCESS + hotkeyText;
                            app->m_eventHandler->ShowNotification(app->m_strings.APP_NAME.c_str(), message.c_str());
                        } else {
                            UINT defaultModifiers = MOD_CONTROL | MOD_ALT;
                            UINT defaultKey = 'R';
                            RegisterHotKey(hwnd, Constants::ID_TRAYICON, defaultModifiers, defaultKey);
                            app->m_configManager->SetHotkeyModifiers(defaultModifiers);
                            app->m_configManager->SetHotkeyKey(defaultKey);
                            app->m_configManager->SaveHotkeyConfig();
                            app->m_eventHandler->ShowNotification(app->m_strings.APP_NAME.c_str(), 
                                app->m_strings.HOTKEY_SET_FAILED.c_str());
                        }
                    }
                }
                return 0;
            }

            case WM_COMMAND: {
                if (!app) return 0;
                WORD cmd = LOWORD(wParam);
                if (cmd >= Constants::ID_RATIO_BASE && cmd < Constants::ID_RATIO_BASE + 1000) {
                    app->m_eventHandler->HandleRatioSelect(cmd);
                } else if (cmd >= Constants::ID_RESOLUTION_BASE && cmd < Constants::ID_RESOLUTION_BASE + 1000) {
                    app->m_eventHandler->HandleResolutionSelect(cmd);
                } else if (cmd >= Constants::ID_WINDOW_BASE && cmd <= Constants::ID_WINDOW_MAX) {
                    app->m_eventHandler->HandleWindowSelect(cmd);
                } else {
                    switch (cmd) {
                        case Constants::ID_CONFIG:
                            app->m_eventHandler->OpenConfigFile();
                            break;
                        case Constants::ID_HOTKEY:
                            app->m_eventHandler->SetHotkey();
                            break;
                        case Constants::ID_EXIT:
                            DestroyWindow(hwnd);
                            break;
                        case Constants::ID_RESET:
                            app->m_eventHandler->ResetWindowSize();
                            break;
                        case Constants::ID_LANG_ZH_CN:
                            app->m_eventHandler->ChangeLanguage(Constants::LANG_ZH_CN);
                            break;
                        case Constants::ID_LANG_EN_US:
                            app->m_eventHandler->ChangeLanguage(Constants::LANG_EN_US);
                            break;
                        case Constants::ID_AUTOHIDE_TASKBAR:
                            app->m_eventHandler->ToggleTaskbarAutoHide();
                            break;
                        case Constants::ID_LOWER_TASKBAR:
                            app->m_eventHandler->ToggleTaskbarLower();
                            break;
                        case Constants::ID_FLOATING_WINDOW:
                            app->m_eventHandler->ToggleFloatingWindow();
                            break;
                        case Constants::ID_OVERLAY_WINDOW:
                            app->m_eventHandler->ToggleOverlayWindow();
                            break;
                        case Constants::ID_TOGGLE_WINDOW_VISIBILITY:
                            if (app->m_menuWindow) {
                                app->m_menuWindow->ToggleVisibility();
                            }
                            break;
                        case Constants::ID_CAPTURE_WINDOW:
                            app->m_eventHandler->HandleScreenshot();
                            break;
                        case Constants::ID_OPEN_SCREENSHOT:
                            app->m_eventHandler->HandleOpenScreenshot();
                            break;
                        case Constants::ID_PREVIEW_WINDOW:
                            app->m_eventHandler->TogglePreviewWindow();
                            break;
                        case Constants::ID_LETTERBOX_WINDOW:
                            app->m_eventHandler->ToggleLetterboxMode();
                            break;
                        case Constants::ID_USER_GUIDE:
                            ShellExecute(NULL, TEXT("open"), 
                                (app->m_language == Constants::LANG_EN_US) ? Constants::DOC_URL_EN : Constants::DOC_URL_ZH,
                                NULL, NULL, SW_SHOWNORMAL);
                            break;
                    }
                }
                return 0;
            }

            case WM_USER + 1: {  // Constants::WM_TRAYICON
                if (app) {
                    if (lParam == WM_LBUTTONDBLCLK) {
                        app->m_eventHandler->HandleTrayIconDblClick();
                        app->m_lastTrayClickTime = GetTickCount64();
                    } else if (lParam == WM_LBUTTONUP) {
                        // 获取当前时间
                        ULONGLONG currentTime = GetTickCount64();
                        // 获取系统双击时间
                        int doubleClickTime = GetDoubleClickTime();
                        // 如果与上次点击时间间隔大于双击时间，才处理单击
                        if (currentTime - app->m_lastTrayClickTime > (ULONGLONG)doubleClickTime) {
                            app->m_eventHandler->ShowWindowSelectionMenu();
                        }
                        app->m_lastTrayClickTime = currentTime;
                    } else if (lParam == WM_RBUTTONUP) {
                        app->m_eventHandler->ShowWindowSelectionMenu();
                    }
                }
                return 0;
            }

            case WM_HOTKEY: {
                if (app && wParam == Constants::ID_TRAYICON) {
                    if (app->m_configManager->GetUseFloatingWindow()) {
                        if (app->m_menuWindow) {
                            app->m_menuWindow->ToggleVisibility();
                        }
                    } else {
                        POINT pt;
                        GetCursorPos(&pt);
                        app->m_eventHandler->ShowQuickMenu(pt);
                    }
                }
                return 0;
            }

            case Constants::WM_SHOW_PENDING_NOTIFICATIONS: {
                if (app) {
                    // 开始显示通知的索引
                    app->m_currentNotificationIndex = 0;
                    // 设置定时器，每隔一段时间显示一个通知
                    SetTimer(app->m_hwnd, Constants::NOTIFICATION_TIMER_ID, 1000, NULL);
                }
                return 0;
            }

            case Constants::WM_SET_HOTKEY_MODE: {
                if (app) {
                    app->m_hotkeySettingMode = true;
                }
                return 0;
            }

            case WM_TIMER: {
                if (!app) return 0;

                if (wParam == Constants::NOTIFICATION_TIMER_ID) {
                    if (app->m_currentNotificationIndex < app->m_pendingNotifications.size()) {
                        const auto& notification = app->m_pendingNotifications[app->m_currentNotificationIndex];
                        if (app->m_notificationManager) {
                            app->m_notificationManager->ShowNotification(
                                notification.title.c_str(),
                                notification.message.c_str(),
                                notification.type
                            );
                        }
                        app->m_currentNotificationIndex++;
                    } else {
                        // 所有通知已显示，停止定时器
                        KillTimer(app->m_hwnd, Constants::NOTIFICATION_TIMER_ID);
                        app->m_pendingNotifications.clear();
                    }
                    return 0;
                }
                return 0;
            }

            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }

            case Constants::WM_PREVIEW_RCLICK: {
                if (app) {
                    POINT pt;
                    GetCursorPos(&pt);
                    app->m_eventHandler->ShowQuickMenu(pt);
                }
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
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
