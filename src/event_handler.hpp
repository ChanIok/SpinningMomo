#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include "constants.hpp"
#include "window_utils.hpp"
#include "notification_manager.hpp"
#include "config_manager.hpp"
#include "menu_window.hpp"
#include "preview_window.hpp"
#include "overlay_window.hpp"
#include "letterbox_window.hpp"
#include "tray_icon.hpp"

class EventHandler {
private:
    // 必要的引用
    ConfigManager* m_configManager;
    NotificationManager* m_notificationManager;
    LocalizedStrings m_strings;
    MenuWindow* m_menuWindow;
    PreviewWindow* m_previewWindow;
    OverlayWindow* m_overlayWindow;
    LetterboxWindow* m_letterboxWindow;
    TrayIcon* m_trayIcon;
    
    // 必要的状态变量引用
    bool& m_isPreviewEnabled;
    bool& m_isOverlayEnabled;
    bool& m_isLetterboxEnabled;
    size_t& m_currentRatioIndex;
    size_t& m_currentResolutionIndex;
    std::vector<AspectRatio>& m_ratios;
    std::vector<ResolutionPreset>& m_resolutions;
    std::wstring& m_language;
    std::vector<std::pair<HWND, std::wstring>>& m_windows;
    HWND m_mainWindow;
    bool m_isScreenCaptureSupported;
    
    // 热键相关
    WORD m_hotkeyId;
    bool m_hotkeyRegistered;
    bool m_hotkeySettingMode;
public:
    EventHandler(
        HWND mainWindow,
        ConfigManager* configManager,
        NotificationManager* notificationManager,
        const LocalizedStrings& strings,
        MenuWindow* menuWindow,
        PreviewWindow* previewWindow,
        OverlayWindow* overlayWindow,
        LetterboxWindow* letterboxWindow,
        TrayIcon* trayIcon,
        bool& isPreviewEnabled,
        bool& isOverlayEnabled,
        bool& isLetterboxEnabled,
        size_t& currentRatioIndex,
        size_t& currentResolutionIndex,
        std::vector<AspectRatio>& ratios,
        std::vector<ResolutionPreset>& resolutions,
        std::wstring& language,
        std::vector<std::pair<HWND, std::wstring>>& windows,
        bool isScreenCaptureSupported
    );
    
    ~EventHandler();

    // 通知相关
    void ShowNotification(const wchar_t* title, const wchar_t* message, bool isError = false);
    
    // 窗口操作相关
    HWND FindTargetWindow();
    bool ApplyWindowTransform(HWND hwnd, int& outWidth, int& outHeight);
    void ResetWindowSize();
    void HandleScreenshot();
    void HandleOpenScreenshot();
    
    // 菜单相关
    void HandleWindowSelect(int id);
    void HandleRatioSelect(UINT id);
    void HandleResolutionSelect(UINT id);
    void ShowWindowSelectionMenu();
    void ShowQuickMenu(const POINT& pt);
    void HandleTrayIconDblClick();
    
    // 功能切换相关
    void TogglePreviewWindow();
    void ToggleOverlayWindow();
    void ToggleLetterboxMode();
    void ToggleFloatingWindow();
    void ToggleTaskbarAutoHide();
    void ToggleTaskbarLower();
    void ToggleBorderlessMode();
    void UpdateLetterboxState(HWND hwnd, int width, int height);
    void ChangeLanguage(const std::wstring& lang);
    
    // 其他方法
    bool StartWindowCapture(HWND gameWindow, int width, int height);
    void SetHotkey();
    void OpenConfigFile();
    std::wstring GetHotkeyText();
    
    // 热键管理
    bool RegisterHotkey(UINT modifiers, UINT key);
    void UnregisterHotkey();
    void HandleHotkeyTriggered();
    bool IsHotkeySettingMode() const { return m_hotkeySettingMode; }
    void SetHotkeySettingMode(bool mode) { m_hotkeySettingMode = mode; }
    bool HandleKeyDown(WPARAM key);  // 返回true表示已处理
};
