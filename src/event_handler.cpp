#include "event_handler.hpp"
#include "logger.hpp"
#include <shellapi.h>
#include <chrono>
#include <shlwapi.h>

EventHandler::EventHandler(
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
) : m_mainWindow(mainWindow),
    m_configManager(configManager),
    m_notificationManager(notificationManager),
    m_strings(strings),
    m_menuWindow(menuWindow),
    m_previewWindow(previewWindow),
    m_overlayWindow(overlayWindow),
    m_letterboxWindow(letterboxWindow),
    m_trayIcon(trayIcon),
    m_isPreviewEnabled(isPreviewEnabled),
    m_isOverlayEnabled(isOverlayEnabled),
    m_isLetterboxEnabled(isLetterboxEnabled),
    m_currentRatioIndex(currentRatioIndex),
    m_currentResolutionIndex(currentResolutionIndex),
    m_ratios(ratios),
    m_resolutions(resolutions),
    m_language(language),
    m_windows(windows),
    m_isScreenCaptureSupported(isScreenCaptureSupported),
    m_hotkeyId(Constants::ID_TRAYICON),
    m_hotkeyRegistered(false),
    m_hotkeySettingMode(false)
{
}

EventHandler::~EventHandler() {
    UnregisterHotkey();
}

// 显示通知
void EventHandler::ShowNotification(const wchar_t* title, const wchar_t* message, bool isError) {
    if (!m_notificationManager) return;
    m_notificationManager->ShowNotification(
        title,
        message,
        isError ? NotificationWindow::NotificationType::Error 
               : NotificationWindow::NotificationType::Info
    );
}

// 查找目标窗口
HWND EventHandler::FindTargetWindow() {
     return WindowUtils::FindTargetWindow(m_configManager->GetWindowTitle());
}

// 应用窗口变换
bool EventHandler::ApplyWindowTransform(HWND hwnd, int& outWidth, int& outHeight) {
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
        
        if (m_isLetterboxEnabled) {
            UpdateLetterboxState(hwnd, targetRes.width, targetRes.height);
        }
        
        return true;
    } else {
        ShowNotification(m_strings.APP_NAME.c_str(), m_strings.ADJUST_FAILED.c_str());
        return false;
    }
}

// 重置窗口大小
void EventHandler::ResetWindowSize() {
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

// 处理截图
void EventHandler::HandleScreenshot() {
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
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    wchar_t timestamp[20];
    std::tm local_tm;
    localtime_s(&local_tm, &now_time);
    wcsftime(timestamp, 20, L"%Y%m%d_%H%M%S", &local_tm);
    
    // 添加毫秒
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;
    std::wstring filename = std::wstring(timestamp) + 
                           L"_" + std::to_wstring(ms) + L".png";
    
    std::wstring savePath = WindowUtils::GetScreenshotPath() + L"\\" + filename;
    
    WindowUtils::TakeScreenshotAsync(gameWindow, savePath, 
        [this](bool success, const std::wstring& path) {
            if (success) {
                ShowNotification(
                    m_strings.APP_NAME.c_str(),
                    (m_strings.CAPTURE_SUCCESS + path).c_str()
                );
            }
        }
    );
}

// 处理打开截图
void EventHandler::HandleOpenScreenshot() {
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
            } else {
                albumPath = WindowUtils::GetScreenshotPath();
            }
        }
    }

    if (!albumPath.empty()) {
        ShellExecuteW(NULL, L"explore", albumPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

// 处理窗口选择
void EventHandler::HandleWindowSelect(int id) {
    int index = id - Constants::ID_WINDOW_BASE;
    if (index >= 0 && index < m_windows.size()) {
        m_configManager->SetWindowTitle(m_windows[index].second);
        m_configManager->SaveWindowConfig();
        ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.WINDOW_SELECTED.c_str());
    }
}

// 处理宽高比选择
void EventHandler::HandleRatioSelect(UINT id) {
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
void EventHandler::HandleResolutionSelect(UINT id) {
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

// 菜单相关方法
void EventHandler::ShowWindowSelectionMenu() {
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
        m_isOverlayEnabled,
        m_isLetterboxEnabled
    );
}

void EventHandler::ShowQuickMenu(const POINT& pt) {
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

void EventHandler::HandleTrayIconDblClick() {
    if (m_configManager->GetUseFloatingWindow() && m_menuWindow) {
        if (!m_menuWindow->IsVisible()) {
            m_menuWindow->Show();
        }
    }
}

// 功能切换相关
void EventHandler::TogglePreviewWindow() {
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

void EventHandler::ToggleOverlayWindow() {
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
        if (HWND gameWindow = FindTargetWindow()) {
            m_overlayWindow->StartCapture(gameWindow);
        }
    } else {
        m_overlayWindow->StopCapture();
    }

    if (m_menuWindow) {
        m_menuWindow->SetOverlayEnabled(m_isOverlayEnabled);
    }
}

void EventHandler::ToggleLetterboxMode() {
    m_isLetterboxEnabled = !m_isLetterboxEnabled;
    
    // 更新配置并保存
    m_configManager->SetLetterboxEnabled(m_isLetterboxEnabled);
    m_configManager->SaveLetterboxConfig();
    
    if (m_menuWindow) {
        m_menuWindow->SetLetterboxEnabled(m_isLetterboxEnabled);
    }

    // 如果关闭黑边模式，则需要关闭黑边窗口
    if (!m_isLetterboxEnabled && m_letterboxWindow) {
        m_letterboxWindow->Shutdown();
    }

    // 更新叠加层的黑边模式设置
    if (m_overlayWindow) {
        m_overlayWindow->SetLetterboxMode(m_isLetterboxEnabled);
        
        // 如果叠加层已启用且正在捕获，重启捕获以应用新设置
        if (m_isOverlayEnabled && m_overlayWindow->IsCapturing()) {
            HWND gameWindow = FindTargetWindow();
            if (gameWindow) {
                m_overlayWindow->StopCapture();
                m_overlayWindow->StartCapture(gameWindow);
                return;
            }
        }
    }
    
    // 如果开启黑边模式且没有活动的叠加层，则检查是否需要显示黑边窗口
    if (m_isLetterboxEnabled) {
        HWND gameWindow = FindTargetWindow();
        if (gameWindow) {
            RECT rect;
            GetClientRect(gameWindow, &rect);
            UpdateLetterboxState(gameWindow, rect.right, rect.bottom);
        }
    }
}

void EventHandler::ToggleFloatingWindow() {
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

void EventHandler::ToggleTaskbarAutoHide() {
    APPBARDATA abd = {0};
    abd.cbSize = sizeof(APPBARDATA);
    UINT state = (UINT)SHAppBarMessage(ABM_GETSTATE, &abd);
    bool currentAutoHide = (state & ABS_AUTOHIDE) != 0;
    abd.lParam = currentAutoHide ? 0 : ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &abd);
    m_configManager->SetTaskbarAutoHide(!currentAutoHide);
    m_configManager->SaveTaskbarConfig();
}

void EventHandler::ToggleTaskbarLower() {
    m_configManager->SetTaskbarLower(!m_configManager->GetTaskbarLower());
    m_configManager->SaveTaskbarConfig();
}

void EventHandler::ToggleBorderlessMode() {
    HWND gameWindow = FindTargetWindow();
    if (!gameWindow) {
        ShowNotification(m_strings.APP_NAME.c_str(), 
                        m_strings.WINDOW_NOT_FOUND.c_str());
        return;
    }
    
    WindowUtils::ToggleWindowBorder(gameWindow);
}

void EventHandler::UpdateLetterboxState(HWND hwnd, int width, int height) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    bool needLetterbox = m_isLetterboxEnabled && 
                        ((width >= screenWidth && height < screenHeight) || 
                         (height >= screenHeight && width < screenWidth));
    
    if (needLetterbox && m_letterboxWindow) {
        m_letterboxWindow->Show(hwnd);
    } else if (m_letterboxWindow && m_letterboxWindow->IsVisible()) {
        if (m_isOverlayEnabled && m_overlayWindow->IsVisible()) {
            m_letterboxWindow->Shutdown();
            return;
        }
        m_letterboxWindow->Hide();
    }
}

void EventHandler::ChangeLanguage(const std::wstring& lang) {
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

bool EventHandler::StartWindowCapture(HWND gameWindow, int width, int height) {
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

void EventHandler::SetHotkey() {
    m_hotkeySettingMode = true;
    ShowNotification(m_strings.APP_NAME.c_str(), m_strings.HOTKEY_SETTING.c_str());
}

void EventHandler::OpenConfigFile() {
    ShellExecuteW(NULL, L"open", L"notepad.exe", 
                 m_configManager->GetConfigPath().c_str(), NULL, SW_SHOW);
    ShowNotification(m_strings.APP_NAME.c_str(), 
        m_strings.CONFIG_HELP.c_str());
}

std::wstring EventHandler::GetHotkeyText() {
    std::wstring text;
    UINT modifiers = m_configManager->GetHotkeyModifiers();
    UINT key = m_configManager->GetHotkeyKey();

    if (modifiers & MOD_CONTROL) text += L"Ctrl+";
    if (modifiers & MOD_ALT) text += L"Alt+";
    if (modifiers & MOD_SHIFT) text += L"Shift+";
    
    if (key >= 'A' && key <= 'Z') {
        text += static_cast<wchar_t>(key);
    } else {
        switch (key) {
            case VK_F1: text += L"F1"; break;
            case VK_F2: text += L"F2"; break;
            case VK_F3: text += L"F3"; break;
            case VK_F4: text += L"F4"; break;
            case VK_F5: text += L"F5"; break;
            case VK_F6: text += L"F6"; break;
            case VK_F7: text += L"F7"; break;
            case VK_F8: text += L"F8"; break;
            case VK_F9: text += L"F9"; break;
            case VK_F10: text += L"F10"; break;
            case VK_F11: text += L"F11"; break;
            case VK_F12: text += L"F12"; break;
            case VK_NUMPAD0: text += L"Num0"; break;
            case VK_NUMPAD1: text += L"Num1"; break;
            case VK_NUMPAD2: text += L"Num2"; break;
            case VK_NUMPAD3: text += L"Num3"; break;
            case VK_NUMPAD4: text += L"Num4"; break;
            case VK_NUMPAD5: text += L"Num5"; break;
            case VK_NUMPAD6: text += L"Num6"; break;
            case VK_NUMPAD7: text += L"Num7"; break;
            case VK_NUMPAD8: text += L"Num8"; break;
            case VK_NUMPAD9: text += L"Num9"; break;
            case VK_MULTIPLY: text += L"Num*"; break;
            case VK_ADD: text += L"Num+"; break;
            case VK_SUBTRACT: text += L"Num-"; break;
            case VK_DECIMAL: text += L"Num."; break;
            case VK_DIVIDE: text += L"Num/"; break;
            case VK_HOME: text += L"Home"; break;
            case VK_END: text += L"End"; break;
            case VK_PRIOR: text += L"PageUp"; break;
            case VK_NEXT: text += L"PageDown"; break;
            case VK_INSERT: text += L"Insert"; break;
            case VK_DELETE: text += L"Delete"; break;
            case VK_OEM_3: text += L"`"; break;
            default: text += static_cast<wchar_t>(key); break;
        }
    }
    return text;
}

bool EventHandler::RegisterHotkey(UINT modifiers, UINT key) {
    // 确保先卸载之前的热键
    UnregisterHotkey();
    
    bool success = ::RegisterHotKey(m_mainWindow, Constants::ID_TRAYICON, modifiers, key);
    
    if (success) {
        m_hotkeyRegistered = true;
        // 更新配置
        m_configManager->SetHotkeyModifiers(modifiers);
        m_configManager->SetHotkeyKey(key);
        m_configManager->SaveHotkeyConfig();
    }
    
    return success;
}

void EventHandler::UnregisterHotkey() {
    if (m_hotkeyRegistered) {
        ::UnregisterHotKey(m_mainWindow, Constants::ID_TRAYICON);
        m_hotkeyRegistered = false;
    }
}

void EventHandler::HandleHotkeyTriggered() {
    if (m_configManager->GetUseFloatingWindow()) {
        if (m_menuWindow) {
            m_menuWindow->ToggleVisibility();
        }
    } else {
        POINT pt;
        GetCursorPos(&pt);
        ShowQuickMenu(pt);
    }
}

bool EventHandler::HandleKeyDown(WPARAM key) {
    if (!m_hotkeySettingMode) {
        return false;
    }
    
    // 获取修饰键状态
    UINT modifiers = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;

    // 如果按下了非修饰键
    if (key != VK_CONTROL && key != VK_SHIFT && key != VK_MENU) {
        m_hotkeySettingMode = false;

        // 尝试注册新热键
        if (RegisterHotkey(modifiers, static_cast<UINT>(key))) {
            std::wstring hotkeyText = GetHotkeyText();
            std::wstring message = m_strings.HOTKEY_SET_SUCCESS + hotkeyText;
            ShowNotification(m_strings.APP_NAME.c_str(), message.c_str());
        } else {
            UINT defaultModifiers = MOD_CONTROL | MOD_ALT;
            UINT defaultKey = 'R';
            RegisterHotkey(defaultModifiers, defaultKey);
            ShowNotification(m_strings.APP_NAME.c_str(), 
                m_strings.HOTKEY_SET_FAILED.c_str());
        }
        return true;
    }
    
    return false;
}
