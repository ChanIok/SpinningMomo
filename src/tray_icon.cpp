#include "tray_icon.hpp"
#include "window_utils.hpp"
#include <windowsx.h>
#include <algorithm>
#include <tchar.h>

TrayIcon::TrayIcon(HWND hwnd) : m_hwnd(hwnd) {
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

TrayIcon::~TrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &m_nid);
    if (m_nid.hIcon) DestroyIcon(m_nid.hIcon);
}

bool TrayIcon::Create() {
    return Shell_NotifyIcon(NIM_ADD, &m_nid) != FALSE;
}

void TrayIcon::ShowBalloon(const TCHAR* title, const TCHAR* message) {
    try {
        m_nid.uFlags = NIF_INFO;
        if (FAILED(StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), title)) ||
            FAILED(StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), message))) {
            // 如果复制失败，使用安全的默认消息
            StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), TEXT("Notice"));
            StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), TEXT("An error occurred"));
        }
        m_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    } catch (...) {
        // 确保即使出错也能恢复正常状态
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
}

void TrayIcon::UpdateTip(const TCHAR* tip) {
    StringCchCopy(m_nid.szTip, _countof(m_nid.szTip), tip);
    Shell_NotifyIcon(NIM_MODIFY, &m_nid);
}

void TrayIcon::ShowContextMenu(
    const std::vector<std::pair<HWND, std::wstring>>& windows,
    const std::wstring& currentTitle,
    const std::vector<AspectRatio>& ratios,
    size_t currentRatioIndex,
    const std::vector<ResolutionPreset>& resolutions,
    size_t currentResolutionIndex,
    const LocalizedStrings& strings,
    bool taskbarAutoHide,
    bool taskbarLower,
    const std::wstring& language,
    bool useFloatingWindow,
    bool isFloatingWindowVisible,
    bool previewEnabled) {
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // 添加窗口选择子菜单
    HMENU hWindowMenu = CreateWindowSelectionSubmenu(windows, currentTitle, strings);
    if (hWindowMenu) {
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                  (UINT_PTR)hWindowMenu, strings.SELECT_WINDOW.c_str());
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    }

    // 添加比例子菜单
    HMENU hRatioMenu = CreateRatioSubmenu(ratios, currentRatioIndex, strings);
    if (hRatioMenu) {
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                  (UINT_PTR)hRatioMenu, strings.WINDOW_RATIO.c_str());
    }

    // 添加分辨率子菜单
    HMENU hSizeMenu = CreateResolutionSubmenu(resolutions, currentResolutionIndex, strings);
    if (hSizeMenu) {
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                  (UINT_PTR)hSizeMenu, strings.RESOLUTION.c_str());
    }

    // 添加重置选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_RESET, strings.RESET_WINDOW.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加截图选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_CAPTURE_WINDOW, strings.CAPTURE_WINDOW.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_OPEN_SCREENSHOT, strings.OPEN_SCREENSHOT.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    
    // 添加设置选项
    AddSettingsItems(hMenu, taskbarAutoHide, taskbarLower, useFloatingWindow, 
                    isFloatingWindowVisible, previewEnabled, strings);

    // 添加语言子菜单
    HMENU hLangMenu = CreateLanguageSubmenu(language, strings);
    if (hLangMenu) {
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                  (UINT_PTR)hLangMenu, strings.LANGUAGE.c_str());
    }

    // 添加配置和退出选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_CONFIG, strings.OPEN_CONFIG.c_str());
    
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    // 添加使用指南选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_USER_GUIDE, strings.USER_GUIDE.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_EXIT, strings.EXIT.c_str());

    // 显示菜单
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                  pt.x, pt.y, 0, m_hwnd, NULL);

    DestroyMenu(hMenu);
}

void TrayIcon::ShowQuickMenu(
    const POINT& pt,
    const std::vector<AspectRatio>& ratios,
    size_t currentRatioIndex,
    const std::vector<ResolutionPreset>& resolutions,
    size_t currentResolutionIndex,
    const LocalizedStrings& strings,
    bool taskbarAutoHide,
    bool previewEnabled) {
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // 添加比例子菜单
    HMENU hRatioMenu = CreateRatioSubmenu(ratios, currentRatioIndex, strings);
    if (hRatioMenu) {
        for (size_t i = 0; i < ratios.size(); ++i) {
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == currentRatioIndex) {
                flags |= MF_CHECKED;
            }
            InsertMenu(hMenu, -1, flags,
                      Constants::ID_RATIO_BASE + i, ratios[i].name.c_str());
        }
    }

    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加分辨率子菜单
    HMENU hSizeMenu = CreateResolutionSubmenu(resolutions, currentResolutionIndex, strings);
    if (hSizeMenu) {
        for (size_t i = 0; i < resolutions.size(); ++i) {
            const auto& preset = resolutions[i];
            TCHAR menuText[256];
            if (preset.baseWidth == 0 && preset.baseHeight == 0) {
                // 如果是默认选项，不显示像素数
                _stprintf_s(menuText, _countof(menuText), TEXT("%s"), preset.name.c_str());
            } else {
                _stprintf_s(menuText, _countof(menuText), TEXT("%s (%.1fM)"), 
                    preset.name.c_str(), 
                    preset.totalPixels / 1000000.0);
            }
            
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == currentResolutionIndex) {
                flags |= MF_CHECKED;
            }
            InsertMenu(hMenu, -1, flags,
                      Constants::ID_RESOLUTION_BASE + i, menuText);
        }
    }

    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加截图选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_CAPTURE_WINDOW, strings.CAPTURE_WINDOW.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加预览窗口选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (previewEnabled ? MF_CHECKED : 0),
              Constants::ID_PREVIEW_WINDOW, strings.PREVIEW_WINDOW.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加重置选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
              Constants::ID_RESET, strings.RESET_WINDOW.c_str());

    // 显示菜单
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                  pt.x, pt.y, 0, m_hwnd, NULL);

    DestroyMenu(hMenu);
}

HMENU TrayIcon::CreateWindowSelectionSubmenu(
    const std::vector<std::pair<HWND, std::wstring>>& windows,
    const std::wstring& currentWindowTitle,
    const LocalizedStrings& strings) {
    
    HMENU hWindowMenu = CreatePopupMenu();
    if (!hWindowMenu) return NULL;

    int id = Constants::ID_WINDOW_BASE;
    for (const auto& window : windows) {
        UINT flags = MF_BYPOSITION | MF_STRING;
        if (window.second == currentWindowTitle) {  // 简化比较逻辑
            flags |= MF_CHECKED;
        }
        InsertMenu(hWindowMenu, -1, flags, id++, window.second.c_str());
    }

    return hWindowMenu;
}

HMENU TrayIcon::CreateRatioSubmenu(
    const std::vector<AspectRatio>& ratios,
    size_t currentRatioIndex,
    const LocalizedStrings& strings) {
    
    HMENU hRatioMenu = CreatePopupMenu();
    if (!hRatioMenu) return NULL;

    for (size_t i = 0; i < ratios.size(); ++i) {
        UINT flags = MF_BYPOSITION | MF_STRING;
        if (i == currentRatioIndex) {
            flags |= MF_CHECKED;
        }
        InsertMenu(hRatioMenu, -1, flags, Constants::ID_RATIO_BASE + i, ratios[i].name.c_str());
    }

    return hRatioMenu;
}

HMENU TrayIcon::CreateResolutionSubmenu(
    const std::vector<ResolutionPreset>& resolutions,
    size_t currentResolutionIndex,
    const LocalizedStrings& strings) {
    
    HMENU hSizeMenu = CreatePopupMenu();
    if (!hSizeMenu) return NULL;

    for (size_t i = 0; i < resolutions.size(); ++i) {
        const auto& preset = resolutions[i];
        TCHAR menuText[256];
        if (preset.baseWidth == 0 && preset.baseHeight == 0) {
            // 如果是默认选项，不显示像素数
            _stprintf_s(menuText, _countof(menuText), TEXT("%s"), preset.name.c_str());
        } else {
            _stprintf_s(menuText, _countof(menuText), TEXT("%s (%.1fM)"), 
                preset.name.c_str(), 
                preset.totalPixels / 1000000.0);
        }
        
        UINT flags = MF_BYPOSITION | MF_STRING;
        if (i == currentResolutionIndex) {
            flags |= MF_CHECKED;
        }
        InsertMenu(hSizeMenu, -1, flags, Constants::ID_RESOLUTION_BASE + i, menuText);
    }

    return hSizeMenu;
}

HMENU TrayIcon::CreateLanguageSubmenu(
    const std::wstring& currentLanguage,
    const LocalizedStrings& strings) {
    
    HMENU hLangMenu = CreatePopupMenu();
    if (!hLangMenu) return NULL;

    InsertMenu(hLangMenu, -1, MF_BYPOSITION | MF_STRING | 
              (currentLanguage == Constants::LANG_ZH_CN ? MF_CHECKED : 0),
              Constants::ID_LANG_ZH_CN, strings.CHINESE.c_str());
    InsertMenu(hLangMenu, -1, MF_BYPOSITION | MF_STRING |
              (currentLanguage == Constants::LANG_EN_US ? MF_CHECKED : 0),
              Constants::ID_LANG_EN_US, strings.ENGLISH.c_str());

    return hLangMenu;
}

void TrayIcon::AddSettingsItems(
    HMENU hMenu,
    bool taskbarAutoHide,
    bool taskbarLower,
    bool useFloatingWindow,
    bool isFloatingWindowVisible,
    bool previewEnabled,
    const LocalizedStrings& strings) {
    
    // 任务栏自动隐藏选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (taskbarAutoHide ? MF_CHECKED : 0),
              Constants::ID_AUTOHIDE_TASKBAR, strings.TASKBAR_AUTOHIDE.c_str());
              
    // 任务栏置底选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (taskbarLower ? MF_CHECKED : 0),
              Constants::ID_LOWER_TASKBAR, strings.TASKBAR_LOWER.c_str());
              
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 热键设置
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_HOTKEY, strings.MODIFY_HOTKEY.c_str());

    // 预览窗口选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (previewEnabled ? MF_CHECKED : 0),
              Constants::ID_PREVIEW_WINDOW, strings.PREVIEW_WINDOW.c_str());
              
    // 浮窗模式选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (useFloatingWindow ? MF_CHECKED : 0),
              Constants::ID_FLOATING_WINDOW, strings.FLOATING_MODE.c_str());

    // 如果启用了浮窗模式，添加浮窗显示/隐藏控制选项
    if (useFloatingWindow) {
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING,
                  Constants::ID_TOGGLE_WINDOW_VISIBILITY,
                  (isFloatingWindowVisible ? strings.CLOSE_WINDOW.c_str() : strings.SHOW_WINDOW.c_str()));
    }
} 