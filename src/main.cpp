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
#include "logger.hpp"

// 主应用程序类
class SpinningMomoApp {
public:
    SpinningMomoApp() = default;
    ~SpinningMomoApp() {
        if (m_configManager) {
            m_configManager->SaveAllConfigs();
        }
        UnregisterHotKey(m_hwnd, 1);
    }

    bool Initialize(HINSTANCE hInstance) {
        Logger::GetInstance().Initialize();
        LOG_INFO("Program initialization started");

        // 记录系统信息
        LogSystemInfo();

        m_configManager = std::make_unique<ConfigManager>();
        m_configManager->Initialize();
        
        // 加载配置
        m_configManager->LoadAllConfigs();

        // 创建通知管理器
        m_notificationManager = std::make_unique<NotificationManager>(hInstance);

        // 初始化字符串
        m_language = m_configManager->GetLanguage();
        m_strings = (m_language == Constants::LANG_EN_US) ? EN_US : ZH_CN;

        // 检查屏幕捕获功能是否可用
        m_isScreenCaptureSupported = WindowUtils::IsWindowsCaptureSupported();
        LOG_INFO("Screen capture feature is %s", m_isScreenCaptureSupported ? "available" : "not available");

        // 初始化预设数据
        InitializeRatios();
        InitializeResolutions();
        
        // 根据配置重建宽高比和分辨率列表
        ConfigLoadResult ratioResult = m_configManager->BuildRatiosFromConfig(m_ratios, m_ratios, m_strings);
        if (!ratioResult.success) {
            LOG_ERROR("Failed to load aspect ratio configuration: %s", ratioResult.errorDetails.c_str());
            m_notificationManager->ShowNotification(m_strings.APP_NAME.c_str(), ratioResult.errorDetails);
        }
        
        ConfigLoadResult resolutionResult = m_configManager->BuildResolutionsFromConfig(m_resolutions, m_resolutions, m_strings);
        if (!resolutionResult.success) {
            LOG_ERROR("Failed to load resolution configuration: %s", resolutionResult.errorDetails.c_str());
            m_notificationManager->ShowNotification(m_strings.APP_NAME.c_str(), resolutionResult.errorDetails);
        }

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

        // 创建菜单窗口
        m_menuWindow = std::make_unique<MenuWindow>(hInstance);
        // 设置菜单项显示配置
        m_menuWindow->SetMenuItemsToShow(m_configManager->GetMenuItemsToShow());
        if (!m_menuWindow->Create(m_hwnd, m_ratios, m_resolutions, m_strings,
                            m_currentRatioIndex, m_currentResolutionIndex,
                            m_isPreviewEnabled, m_isOverlayEnabled)) {
            LOG_ERROR("Failed to create menu window");
            return false;
        }

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
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.HOTKEY_REGISTER_FAILED.c_str());
        } else {
            // 只在热键注册成功时显示启动提示
            std::wstring hotkeyText = GetHotkeyText();
            std::wstring message = m_strings.STARTUP_MESSAGE + hotkeyText + m_strings.STARTUP_MESSAGE_SUFFIX;
            ShowNotification(m_strings.APP_NAME.c_str(), message.c_str());
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

    // 处理窗口选择
    void HandleWindowSelect(int id) {
        int index = id - Constants::ID_WINDOW_BASE;
        if (index >= 0 && index < m_windows.size()) {
            m_configManager->SetWindowTitle(m_windows[index].second);
            m_configManager->SaveWindowConfig();
            ShowNotification(m_strings.APP_NAME.c_str(), 
                    m_strings.WINDOW_SELECTED.c_str());
        }
    }

    // 处理宽高比选择
    void HandleRatioSelect(UINT id) {
        size_t index = id - Constants::ID_RATIO_BASE;
        if (index >= m_ratios.size()) {
            return;
        }
    
        m_currentRatioIndex = index;
    
        if (m_menuWindow) {
            m_menuWindow->SetCurrentRatio(index);
        }
    
        HWND gameWindow = FindTargetWindow();
        if (!gameWindow) {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                            m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }
    
        if (m_isOverlayEnabled && m_overlayWindow) {
            m_overlayWindow->StopCapture(false);
        }
        if (m_isPreviewEnabled && m_previewWindow) {
            m_previewWindow->StopCapture();
        }
    
        int width = 0, height = 0;
        if (ApplyWindowTransform(gameWindow, width, height)) {
            StartWindowCapture(gameWindow, width, height);
        }
    }

    // 处理分辨率选择
    void HandleResolutionSelect(UINT id) {
        size_t index = id - Constants::ID_RESOLUTION_BASE;
        if (index >= m_resolutions.size()) {
            return;
        }
    
        m_currentResolutionIndex = index;
    
        if (m_menuWindow) {
            m_menuWindow->SetCurrentResolution(index);
        }
    
        HWND gameWindow = FindTargetWindow();
        if (!gameWindow) {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                            m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }
    
        if (m_isOverlayEnabled && m_overlayWindow) {
            m_overlayWindow->StopCapture(false);
        }
        if (m_isPreviewEnabled && m_previewWindow) {
            m_previewWindow->StopCapture();
        }
    
        // 应用新分辨率
        int width = 0, height = 0;
        if (ApplyWindowTransform(gameWindow, width, height)) {
            StartWindowCapture(gameWindow, width, height);
        }
    }
    
    // 处理截图
    void HandleScreenshot() {
        // 检查屏幕捕获功能是否可用
        if (!m_isScreenCaptureSupported) {
            ShowNotification(m_strings.APP_NAME.c_str(),
                m_strings.FEATURE_NOT_SUPPORTED.c_str());
            return;
        }

        HWND gameWindow = FindTargetWindow();
        if (!gameWindow) {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }

        // 检查窗口是否最小化
        if (IsIconic(gameWindow)) {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }

        // 使用当前时间生成格式化的文件名
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto local_time = std::localtime(&time);
        
        wchar_t timestamp[32];
        std::wcsftime(timestamp, sizeof(timestamp)/sizeof(wchar_t), 
                     L"%Y%m%d_%H%M%S", local_time);
        
        // 添加毫秒
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;
        std::wstring filename = std::wstring(timestamp) + 
                               L"_" + std::to_wstring(ms) + L".png";
        
        std::wstring savePath = WindowUtils::GetScreenshotPath() + L"\\" + filename;
        
        if (!WindowUtils::CaptureWindow(gameWindow, [this, savePath](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
            if (WindowUtils::SaveFrameToFile(texture.Get(), savePath)) {
                ShowNotification(
                    m_strings.APP_NAME.c_str(),
                    (m_strings.CAPTURE_SUCCESS + savePath).c_str()
                );
            }
        })) {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.WINDOW_NOT_FOUND.c_str());
        }
    }

    // 处理打开截图
    void HandleOpenScreenshot() {
        std::wstring albumPath = m_configManager->GetGameAlbumPath();
        
        if (albumPath.empty() || !PathFileExistsW(albumPath.c_str())) {
            HWND gameWindow = FindTargetWindow();
            if (!gameWindow) {
                // 如果找不到游戏窗口，打开程序的截图目录
                albumPath = WindowUtils::GetScreenshotPath();
            } else {
                albumPath = WindowUtils::GetGameScreenshotPath(gameWindow);
                if (!albumPath.empty()) {
                    m_configManager->SetGameAlbumPath(albumPath);
                    m_configManager->SaveGameAlbumConfig();
                }
            }
        }

        if (!albumPath.empty()) {
            ShellExecuteW(NULL, L"explore", albumPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    }

    // 设置热键
    void SetHotkey() {
        UnregisterHotKey(m_hwnd, Constants::ID_TRAYICON);
        m_hotkeySettingMode = true;
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.HOTKEY_SETTING.c_str());
    }

    // 切换任务栏自动隐藏
    void ToggleTaskbarAutoHide() {
        APPBARDATA abd = {0};
        abd.cbSize = sizeof(APPBARDATA);
        UINT state = (UINT)SHAppBarMessage(ABM_GETSTATE, &abd);
        bool currentAutoHide = (state & ABS_AUTOHIDE) != 0;
        abd.lParam = currentAutoHide ? 0 : ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        m_configManager->SetTaskbarAutoHide(!currentAutoHide);
        m_configManager->SaveTaskbarConfig();
    }

    // 切换任务栏是否在底部
    void ToggleTaskbarLower() {
        m_configManager->SetTaskbarLower(!m_configManager->GetTaskbarLower());
        m_configManager->SaveTaskbarConfig();
    }

    // 切换浮动窗口
    void ToggleFloatingWindow() {
        bool wasEnabled = m_configManager->GetUseFloatingWindow();
        bool newState = !wasEnabled;
        m_configManager->SetUseFloatingWindow(newState);
        m_configManager->SaveMenuConfig();

        if (m_menuWindow) {
            if (newState && !wasEnabled) {
                m_menuWindow->Show();
            } else if (!newState && m_menuWindow->IsVisible()) {
                m_menuWindow->Hide();
            }
        }
    }

    // 切换预览窗口
    void TogglePreviewWindow() {
        if (!m_isScreenCaptureSupported) {
            ShowNotification(m_strings.APP_NAME.c_str(),
                m_strings.FEATURE_NOT_SUPPORTED.c_str());
            return;
        }

        // 先关闭叠加层
        if (!m_isPreviewEnabled && m_isOverlayEnabled) {
            m_isOverlayEnabled = false;
            m_overlayWindow->StopCapture();
            if (m_menuWindow) {
                m_menuWindow->SetOverlayEnabled(false);
            }
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.FEATURE_CONFLICT.c_str());
        }

        m_isPreviewEnabled = !m_isPreviewEnabled;
        
        if (m_isPreviewEnabled) {
            if (HWND gameWindow = FindTargetWindow()) {
                m_previewWindow->StartCapture(gameWindow);
            }
        } else {
            m_previewWindow->StopCapture();
        }
        
        if (m_menuWindow) {
            m_menuWindow->SetPreviewEnabled(m_isPreviewEnabled);
        }
    }

    // 切换叠加层窗口
    void ToggleOverlayWindow() {
        if (!m_isScreenCaptureSupported) {
            ShowNotification(m_strings.APP_NAME.c_str(),
                m_strings.FEATURE_NOT_SUPPORTED.c_str());
            return;
        }

        // 如果现在要启用叠加层，但预览窗口已启用，先关闭预览窗口
        if (!m_isOverlayEnabled && m_isPreviewEnabled) {
            m_isPreviewEnabled = false;
            m_previewWindow->StopCapture();
            if (m_menuWindow) {
                m_menuWindow->SetPreviewEnabled(false);
            }
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.FEATURE_CONFLICT.c_str());
        }

        m_isOverlayEnabled = !m_isOverlayEnabled;
        
        if (m_isOverlayEnabled) {
            if (HWND gameWindow = WindowUtils::FindTargetWindow()) {
                m_overlayWindow->StartCapture(gameWindow);
            }
        } else {
            m_overlayWindow->StopCapture();
        }

        if (m_menuWindow) {
            m_menuWindow->SetOverlayEnabled(m_isOverlayEnabled);
        }
    }

    // 切换语言
    void ChangeLanguage(const std::wstring& lang) {
        if (m_language != lang) {
            m_language = lang;
            m_strings = (lang == Constants::LANG_EN_US) ? EN_US : ZH_CN;
            m_configManager->SetLanguage(m_language);
            m_configManager->SaveLanguageConfig();
            
            if (m_trayIcon) {
                m_trayIcon->UpdateTip(m_strings.APP_NAME.c_str());
            }
            if (m_menuWindow) {
                m_menuWindow->UpdateMenuItems(m_strings);
            }
        }
    }

    // 重置窗口大小
    void ResetWindowSize() {
        HWND gameWindow = FindTargetWindow();
        if (!gameWindow) {
            ShowNotification(m_strings.APP_NAME.c_str(), 
                            m_strings.WINDOW_NOT_FOUND.c_str());
            return;
        }
        m_currentRatioIndex = SIZE_MAX;
        m_currentResolutionIndex = 0;
        
        if (m_menuWindow) {
            m_menuWindow->SetCurrentRatio(m_currentRatioIndex);
            m_menuWindow->SetCurrentResolution(m_currentResolutionIndex);
        }
        if (m_isPreviewEnabled && m_previewWindow) {
            m_previewWindow->StopCapture();
        }
        if (m_isOverlayEnabled && m_overlayWindow) {
            m_overlayWindow->StopCapture();
        }
        int width = 0, height = 0;
        if (ApplyWindowTransform(gameWindow, width, height)) {
            StartWindowCapture(gameWindow, width, height);
        }
    } 

    // 打开配置文件
    void OpenConfigFile() {
        ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), 
                    m_configManager->GetConfigPath().c_str(), NULL, SW_SHOW);
        ShowNotification(m_strings.APP_NAME.c_str(), 
            m_strings.CONFIG_HELP.c_str());
    }

    // 菜单相关方法
    void ShowWindowSelectionMenu() {
        // 更新窗口列表
        m_windows = WindowUtils::GetWindows();
        
        m_trayIcon->ShowContextMenu(
            m_windows,
            m_configManager->GetWindowTitle(),
            m_ratios,
            m_currentRatioIndex,
            m_resolutions,
            m_currentResolutionIndex,
            m_strings,
            m_configManager->GetTaskbarAutoHide(),
            m_configManager->GetTaskbarLower(),
            m_language,
            m_configManager->GetUseFloatingWindow(),
            m_menuWindow && m_menuWindow->IsVisible(),
            m_isPreviewEnabled,
            m_isOverlayEnabled
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
            m_configManager->GetTaskbarAutoHide(),
            m_isPreviewEnabled
        );
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
                            
                            std::wstring hotkeyText = app->GetHotkeyText();
                            std::wstring message = app->m_strings.HOTKEY_SET_SUCCESS + hotkeyText;
                            app->ShowNotification(app->m_strings.APP_NAME.c_str(), message.c_str());
                        } else {
                            UINT defaultModifiers = MOD_CONTROL | MOD_ALT;
                            UINT defaultKey = 'R';
                            RegisterHotKey(hwnd, Constants::ID_TRAYICON, defaultModifiers, defaultKey);
                            app->m_configManager->SetHotkeyModifiers(defaultModifiers);
                            app->m_configManager->SetHotkeyKey(defaultKey);
                            app->m_configManager->SaveHotkeyConfig();
                            app->ShowNotification(app->m_strings.APP_NAME.c_str(), 
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
                        case Constants::ID_OVERLAY_WINDOW:
                            app->ToggleOverlayWindow();
                            break;
                        case Constants::ID_TOGGLE_WINDOW_VISIBILITY:
                            if (app->m_menuWindow) {
                                app->m_menuWindow->ToggleVisibility();
                            }
                            break;
                        case Constants::ID_CAPTURE_WINDOW:
                            app->HandleScreenshot();
                            break;
                        case Constants::ID_OPEN_SCREENSHOT:
                            app->HandleOpenScreenshot();
                            break;
                        case Constants::ID_PREVIEW_WINDOW:
                            app->TogglePreviewWindow();
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
                        app->HandleTrayIconDblClick();
                        app->m_lastTrayClickTime = GetTickCount();
                    } else if (lParam == WM_LBUTTONUP) {
                        // 获取当前时间
                        DWORD currentTime = GetTickCount();
                        // 获取系统双击时间
                        int doubleClickTime = GetDoubleClickTime();
                        // 如果与上次点击时间间隔大于双击时间，才处理单击
                        if (currentTime - app->m_lastTrayClickTime > (DWORD)doubleClickTime) {
                            app->ShowWindowSelectionMenu();
                        }
                        app->m_lastTrayClickTime = currentTime;
                    } else if (lParam == WM_RBUTTONUP) {
                        app->ShowWindowSelectionMenu();
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
                        app->ShowQuickMenu(pt);
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
                    app->ShowQuickMenu(pt);
                }
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void HandleTrayIconDblClick() {
        if (m_configManager->GetUseFloatingWindow() && m_menuWindow) {
            if (!m_menuWindow->IsVisible()) {
                m_menuWindow->Show();
            } else {
                m_menuWindow->Activate();
            }
        }
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

    // 应用状态
    bool m_isPreviewEnabled = false;
    bool m_hotkeySettingMode = false;
    bool m_messageLoopStarted = false;
    bool m_isScreenCaptureSupported = false;
    bool m_isOverlayEnabled = false;
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
    DWORD m_lastTrayClickTime = 0;

    // 待显示通知队列
    struct PendingNotification {
        std::wstring title;
        std::wstring message;
        NotificationWindow::NotificationType type;
    };
    std::vector<PendingNotification> m_pendingNotifications;
    size_t m_currentNotificationIndex = 0;

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
            {TEXT("Default"), 0, 0},    // 默认选项，使用屏幕尺寸计算
            {TEXT("4K"), 3840, 2160},   // 8.3M pixels
            {TEXT("6K"), 5760, 3240},   // 18.7M pixels
            {TEXT("8K"), 7680, 4320},   // 33.2M pixels
            {TEXT("12K"), 11520, 6480}  // 74.6M pixels
        };
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
    
    std::wstring GetHotkeyText() {
        std::wstring text;
        UINT modifiers = m_configManager->GetHotkeyModifiers();
        UINT key = m_configManager->GetHotkeyKey();

        if (modifiers & MOD_CONTROL) text += TEXT("Ctrl+");
        if (modifiers & MOD_ALT) text += TEXT("Alt+");
        if (modifiers & MOD_SHIFT) text += TEXT("Shift+");
        
        if (key >= 'A' && key <= 'Z') {
            text += static_cast<TCHAR>(key);
        } else {
            switch (key) {
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
                case VK_HOME: text += TEXT("Home"); break;
                case VK_END: text += TEXT("End"); break;
                case VK_PRIOR: text += TEXT("PageUp"); break;
                case VK_NEXT: text += TEXT("PageDown"); break;
                case VK_INSERT: text += TEXT("Insert"); break;
                case VK_DELETE: text += TEXT("Delete"); break;
                case VK_OEM_3: text += TEXT("`"); break;
                default: text += static_cast<TCHAR>(key); break;
            }
        }
        return text;
    }

    // 显示通知
    void ShowNotification(const TCHAR* title, const TCHAR* message, bool isError = false) {
        if (!m_notificationManager) return;
        
        // 如果消息循环还没开始，将通知加入待显示队列
        if (!m_messageLoopStarted) {
            m_pendingNotifications.push_back({
                title,
                message,
                isError ? NotificationWindow::NotificationType::Error 
                       : NotificationWindow::NotificationType::Info
            });
            return;
        }

        // 否则直接显示通知
        m_notificationManager->ShowNotification(
            title, 
            message, 
            isError ? NotificationWindow::NotificationType::Error 
                   : NotificationWindow::NotificationType::Info
        );
    }

    // 查找目标窗口
    HWND FindTargetWindow() {
         return WindowUtils::FindTargetWindow(m_configManager->GetWindowTitle());
    }

    // 应用窗口变换
    bool ApplyWindowTransform(HWND hwnd, int& outWidth, int& outHeight) {
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

        LOG_INFO("Target resolution: width=%d, height=%d", targetRes.width, targetRes.height);

        bool resizeSuccess;
        if (m_isOverlayEnabled && m_overlayWindow) {
            resizeSuccess = WindowUtils::ResizeWindow(hwnd, targetRes.width, targetRes.height, 
                m_configManager->GetTaskbarLower(), false);
        } else {
            resizeSuccess = WindowUtils::ResizeWindow(hwnd, targetRes.width, targetRes.height, 
                m_configManager->GetTaskbarLower());
        }
        
        if (resizeSuccess) {
            outWidth = targetRes.width;
            outHeight = targetRes.height;
            return true;
        } else {
            ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_FAILED.c_str());
            return false;
        }
    }

    // 启动窗口捕获，确保互斥性
    bool StartWindowCapture(HWND gameWindow, int width, int height) {
        bool success = true;
    
        if (m_isPreviewEnabled && m_previewWindow) {
            if (width > 0 && height > 0) {
                success = m_previewWindow->StartCapture(gameWindow, width, height);
            } else {
                success = m_previewWindow->StartCapture(gameWindow);
            }
        }
        
        if (m_isOverlayEnabled && m_overlayWindow) {
            if (width > 0 && height > 0) {
                success = success && m_overlayWindow->StartCapture(gameWindow, width, height);
            } else {
                success = success && m_overlayWindow->StartCapture(gameWindow);
            }
        }
        
        return success;
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
