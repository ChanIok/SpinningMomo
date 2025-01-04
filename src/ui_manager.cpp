#include "ui_manager.hpp"
#include "window_utils.hpp"  // 添加 WindowUtils 的头文件
#include <windowsx.h>  // 用于GET_X_LPARAM和GET_Y_LPARAM宏
#include <algorithm>   // 用于std::max
#include <tchar.h>    // 添加 TCHAR 相关函数支持

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


// 新增：TrayIcon的菜单相关方法实现
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

    // 添加重置选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
              Constants::ID_RESET, strings.RESET_WINDOW.c_str());
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // 添加任务栏自动隐藏选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (taskbarAutoHide ? MF_CHECKED : 0),
              Constants::ID_AUTOHIDE_TASKBAR, strings.TASKBAR_AUTOHIDE.c_str());

    // 添加预览窗口选项
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (previewEnabled ? MF_CHECKED : 0),
              Constants::ID_PREVIEW_WINDOW, strings.PREVIEW_WINDOW.c_str());

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

// MenuWindow的静态成员定义
const TCHAR* MenuWindow::MENU_WINDOW_CLASS = TEXT("SpinningMomoMenuClass");

MenuWindow::MenuWindow(HINSTANCE hInstance) : m_hInstance(hInstance) {
    RegisterWindowClass();

    // 获取系统 DPI
    HDC hdc = GetDC(NULL);
    m_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    UpdateDpiDependentResources();
}

void MenuWindow::RegisterWindowClass() {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = MenuWindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = MENU_WINDOW_CLASS;
    wc.hbrBackground = NULL;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);
}

bool MenuWindow::Create(HWND parent, 
                       std::vector<AspectRatio>& ratios,
                       std::vector<ResolutionPreset>& resolutions,
                       const LocalizedStrings& strings,
                       size_t currentRatioIndex,
                       size_t currentResolutionIndex,
                       bool previewEnabled) {
    m_hwndParent = parent;
    m_ratioItems = &ratios;
    m_resolutionItems = &resolutions;
    m_strings = &strings;
    m_currentRatioIndex = currentRatioIndex;
    m_currentResolutionIndex = currentResolutionIndex;
    m_previewEnabled = previewEnabled;
    
    InitializeItems(strings);
    
    // 计算总窗口宽度（三列之和）
    int totalWidth = m_ratioColumnWidth + m_resolutionColumnWidth + m_settingsColumnWidth;
    int windowHeight = CalculateWindowHeight();
    
    // 获取主显示器工作区
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    // 计算窗口位置（屏幕中央）
    int xPos = (workArea.right - workArea.left - totalWidth) / 2;
    int yPos = (workArea.bottom - workArea.top - windowHeight) / 2;
    
    m_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        MENU_WINDOW_CLASS,
        TEXT("SpinningMomo"),
        WS_POPUP | WS_CLIPCHILDREN,
        xPos, yPos,
        totalWidth, windowHeight,
        parent,
        NULL,
        m_hInstance,
        this
    );

    if (!m_hwnd) return false;

    SetLayeredWindowAttributes(m_hwnd, 0, 204, LWA_ALPHA);
    
    // 设置窗口圆角和阴影
    MARGINS margins = {1, 1, 1, 1};  // 四边均匀阴影效果
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    
    BOOL value = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));
    
    // Windows 11 风格的圆角
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;  // 使用小圆角
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    
    return true;
}

void MenuWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void MenuWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool MenuWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void MenuWindow::ToggleVisibility() {
    if (IsVisible()) {
        Hide();
    } else {
        Show();
    }
}

void MenuWindow::SetCurrentRatio(size_t index) {
    m_currentRatioIndex = index;
    InvalidateRect(m_hwnd, NULL, TRUE);
}

void MenuWindow::SetCurrentResolution(size_t index) {
    if (index < m_resolutionItems->size()) {
        m_currentResolutionIndex = index;
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

void MenuWindow::UpdateMenuItems(const LocalizedStrings& strings, bool forceRedraw) {
    m_items.clear();
    InitializeItems(strings);
    if (m_hwnd && forceRedraw) {
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

HWND MenuWindow::GetHwnd() const {
    return m_hwnd;
}

void MenuWindow::InitializeItems(const LocalizedStrings& strings) {
    m_items.clear();
    
    // 添加比例选项
    for (size_t i = 0; i < m_ratioItems->size(); i++) {
        m_items.push_back({(*m_ratioItems)[i].name, ItemType::Ratio, static_cast<int>(i)});
    }

    // 添加分辨率选项
    for (size_t i = 0; i < m_resolutionItems->size(); i++) {
        std::wstring displayText;
        const auto& preset = (*m_resolutionItems)[i];
        if (preset.baseWidth == 0 && preset.baseHeight == 0) {
            // 如果是默认选项，不显示像素数
            displayText = preset.name;
        } else {
            displayText = preset.name + 
                TEXT(" (") + std::to_wstring(preset.totalPixels / 1000000) + TEXT("M)");
        }
        m_items.push_back({displayText, ItemType::Resolution, static_cast<int>(i)});
    }

    // 添加设置选项
    m_items.push_back({strings.CAPTURE_WINDOW, ItemType::CaptureWindow, 0});  // 添加截图选项作为设置组的第一个选项
    m_items.push_back({strings.OPEN_SCREENSHOT, ItemType::OpenScreenshot, 0});  // 添加打开相册选项
    m_items.push_back({strings.PREVIEW_WINDOW, ItemType::PreviewWindow, m_previewEnabled ? 1 : 0});
    m_items.push_back({strings.RESET_WINDOW, ItemType::Reset, 0});
    m_items.push_back({strings.CLOSE_WINDOW, ItemType::Close, 0});
}

LRESULT CALLBACK MenuWindow::MenuWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MenuWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MenuWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<MenuWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT MenuWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DPICHANGED: {
            // 处理 DPI 变化
            m_dpi = HIWORD(wParam);
            UpdateDpiDependentResources();
            // 计算新的窗口大小
            int totalWidth = m_ratioColumnWidth + m_resolutionColumnWidth + m_settingsColumnWidth;
            int windowHeight = CalculateWindowHeight();
            
            // 获取当前窗口位置
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);
            
            int newX = currentRect.left;
            int newY = currentRect.top;
            
            SetWindowPos(hwnd, NULL, newX, newY, totalWidth, windowHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }

        case WM_MOUSELEAVE: {
            OnMouseLeave();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        
        case WM_DESTROY: {
            return 0;
        }
        // 处理鼠标点击事件
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y < m_titleHeight) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_CLOSE:
            Hide();
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MenuWindow::OnPaint(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // 创建双缓冲
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // 设置文本属性，使用 DPI 感知的字体大小
    SetBkMode(memDC, TRANSPARENT);
    HFONT hFont = CreateFont(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

    // 绘制背景
    HBRUSH hBackBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &rect, hBackBrush);
    DeleteObject(hBackBrush);

    // 绘制标题栏
    RECT titleRect = rect;
    titleRect.bottom = m_titleHeight;
    HBRUSH hTitleBrush = CreateSolidBrush(RGB(240, 240, 240));
    FillRect(memDC, &titleRect, hTitleBrush);
    DeleteObject(hTitleBrush);

    // 绘制标题文本
    SetTextColor(memDC, RGB(51, 51, 51));
    titleRect.left += m_textPadding;
    DrawText(memDC, TEXT("SpinningMomo"), -1, &titleRect, 
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);

    // 绘制分隔线
    RECT sepRect = {rect.left, m_titleHeight, rect.right, m_titleHeight + m_separatorHeight};
    HBRUSH hSepBrush = CreateSolidBrush(RGB(229, 229, 229));
    FillRect(memDC, &sepRect, hSepBrush);

    // 计算列宽和位置
    int ratioColumnRight = m_ratioColumnWidth;
    int resolutionColumnRight = ratioColumnRight + m_resolutionColumnWidth;
    
    // 绘制垂直分隔线
    RECT vSepRect1 = {ratioColumnRight, m_titleHeight, ratioColumnRight + m_separatorHeight, rect.bottom};
    RECT vSepRect2 = {resolutionColumnRight, m_titleHeight, resolutionColumnRight + m_separatorHeight, rect.bottom};
    FillRect(memDC, &vSepRect1, hSepBrush);
    FillRect(memDC, &vSepRect2, hSepBrush);
    DeleteObject(hSepBrush);

    // 绘制列表项
    int y = m_titleHeight + m_separatorHeight;  // 直接从分隔线下方开始
    int settingsY = y;  // 为设置列单独维护一个Y坐标
    
    // 遍历所有项目并根据类型分配到不同列
    for (size_t i = 0; i < m_items.size(); i++) {
        const auto& item = m_items[i];
        RECT itemRect;
        
        // 根据项目类型确定绘制位置
        switch (item.type) {
            case ItemType::Ratio:
                itemRect = {0, y, ratioColumnRight, y + m_itemHeight};
                break;
            case ItemType::Resolution:
                itemRect = {ratioColumnRight + m_separatorHeight, y, 
                          resolutionColumnRight, y + m_itemHeight};
                break;
            case ItemType::CaptureWindow:
            case ItemType::OpenScreenshot:
            case ItemType::PreviewWindow:
            case ItemType::Reset:
            case ItemType::Close:
                itemRect = {resolutionColumnRight + m_separatorHeight, settingsY, 
                          rect.right, settingsY + m_itemHeight};
                settingsY += m_itemHeight;
                break;
            default:
                continue;
        }

        // 绘制悬停背景
        if (static_cast<int>(i) == m_hoverIndex) {
            HBRUSH hHoverBrush = CreateSolidBrush(RGB(242, 242, 242));
            FillRect(memDC, &itemRect, hHoverBrush);
            DeleteObject(hHoverBrush);
        }

        // 绘制选中指示器
        bool isSelected = false;
        if (item.type == ItemType::Ratio && item.index == m_currentRatioIndex) {
            isSelected = true;
        } else if (item.type == ItemType::Resolution && item.index == m_currentResolutionIndex) {
            isSelected = true;
        } else if (item.type == ItemType::PreviewWindow && m_previewEnabled) {
            isSelected = true;
        }

        if (isSelected) {
            // 使用专门的比例组指示器宽度
            int indicatorWidth = (item.type == ItemType::Ratio) ? m_ratioIndicatorWidth : m_indicatorWidth;
            RECT indicatorRect = {itemRect.left, itemRect.top, 
                               itemRect.left + indicatorWidth, itemRect.bottom};
            HBRUSH hIndicatorBrush = CreateSolidBrush(RGB(255, 160, 80));
            FillRect(memDC, &indicatorRect, hIndicatorBrush);
            DeleteObject(hIndicatorBrush);
        }

        // 绘制文本
        itemRect.left += m_textPadding + ((item.type == ItemType::Ratio) ? m_ratioIndicatorWidth : m_indicatorWidth);
        SetTextColor(memDC, RGB(51, 51, 51));
        DrawText(memDC, item.text.c_str(), -1, &itemRect, 
                DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        // 只有在同一列中才增加y坐标
        if ((i + 1 < m_items.size()) && (m_items[i + 1].type == item.type)) {
            if (item.type != ItemType::Reset) {
                y += m_itemHeight;
            }
        } else if (i + 1 < m_items.size() && m_items[i + 1].type != item.type) {
            // 如果下一项是不同类型，重置y坐标到列表顶部
            if (m_items[i + 1].type != ItemType::Reset) {
                y = m_titleHeight + m_separatorHeight;  // 重置到分隔线下方
            }
        }
    }

    // 复制到屏幕
    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    // 清理
    SelectObject(memDC, oldFont);
    DeleteObject(hFont);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

void MenuWindow::OnMouseMove(int x, int y) {
        // 计算鼠标悬停的项
        int newHoverIndex = GetItemIndexFromPoint(x, y);
        if (newHoverIndex != m_hoverIndex) {
            m_hoverIndex = newHoverIndex;
            InvalidateRect(m_hwnd, NULL, TRUE);

            // 确保能收到 WM_MOUSELEAVE 消息
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = m_hwnd;
            TrackMouseEvent(&tme);
        }
}

void MenuWindow::OnMouseLeave() {
    if (m_hoverIndex != -1) {
        m_hoverIndex = -1;
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

void MenuWindow::OnLButtonDown(int x, int y) {
    if (m_hoverIndex >= 0 && m_hoverIndex < static_cast<int>(m_items.size())) {
        const auto& item = m_items[m_hoverIndex];
        switch (item.type) {
            case ItemType::Ratio:
                SendMessage(m_hwndParent, WM_COMMAND, 
                          Constants::ID_RATIO_BASE + item.index, 0);
                break;
            case ItemType::Resolution:
                SendMessage(m_hwndParent, WM_COMMAND, 
                          Constants::ID_RESOLUTION_BASE + item.index, 0);
                break;
            case ItemType::CaptureWindow:
                SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_CAPTURE_WINDOW, 0);
                break;
            case ItemType::OpenScreenshot:
                SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_OPEN_SCREENSHOT, 0);
                break;
            case ItemType::PreviewWindow:
                SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_PREVIEW_WINDOW, 0);
                break;
            case ItemType::Reset:
                SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_RESET, 0);
                break;
            case ItemType::Close:
                Hide();  // 直接调用Hide函数关闭窗口
                break;
        }
    }
}

void MenuWindow::UpdateDpiDependentResources() {
    // 更新DPI相关的尺寸
    double scale = static_cast<double>(m_dpi) / 96.0;
    m_itemHeight = static_cast<int>(BASE_ITEM_HEIGHT * scale);
    m_titleHeight = static_cast<int>(BASE_TITLE_HEIGHT * scale);
    m_separatorHeight = static_cast<int>(BASE_SEPARATOR_HEIGHT * scale);
    m_fontSize = static_cast<int>(BASE_FONT_SIZE * scale);
    m_textPadding = static_cast<int>(BASE_TEXT_PADDING * scale);
    m_indicatorWidth = static_cast<int>(BASE_INDICATOR_WIDTH * scale);
    m_ratioIndicatorWidth = static_cast<int>(BASE_RATIO_INDICATOR_WIDTH * scale);
    
    // 更新列宽
    m_ratioColumnWidth = static_cast<int>(BASE_RATIO_COLUMN_WIDTH * scale);
    m_resolutionColumnWidth = static_cast<int>(BASE_RESOLUTION_COLUMN_WIDTH * scale);
    m_settingsColumnWidth = static_cast<int>(BASE_SETTINGS_COLUMN_WIDTH * scale);
}

int MenuWindow::CalculateWindowHeight() {
    // 计算每列的项目数量
    int ratioCount = 0;
    int resolutionCount = 0;
    int settingsCount = 0;

    for (const auto& item : m_items) {
        switch (item.type) {
            case ItemType::Ratio:
                ratioCount++;
                break;
            case ItemType::Resolution:
                resolutionCount++;
                break;
            case ItemType::CaptureWindow:
            case ItemType::OpenScreenshot:
            case ItemType::PreviewWindow:
            case ItemType::Reset:
            case ItemType::Close:
                settingsCount++;
                break;
        }
    }

    // 计算每列的高度
    int ratioHeight = ratioCount * m_itemHeight;
    int resolutionHeight = resolutionCount * m_itemHeight;
    int settingsHeight = settingsCount * m_itemHeight;

    // 找出最大高度
    int maxColumnHeight = ratioHeight;
    if (resolutionHeight > maxColumnHeight) maxColumnHeight = resolutionHeight;
    if (settingsHeight > maxColumnHeight) maxColumnHeight = settingsHeight;

    // 返回总高度
    return m_titleHeight + m_separatorHeight + maxColumnHeight;
}

int MenuWindow::GetItemIndexFromPoint(int x, int y) {
    // 检查是否在标题栏或分隔线区域
    if (y < m_titleHeight + m_separatorHeight) return -1;

    // 计算列的边界
    int ratioColumnRight = m_ratioColumnWidth;
    int resolutionColumnRight = ratioColumnRight + m_resolutionColumnWidth;

    // 确定点击的是哪一列
    ItemType targetType;
    if (x < ratioColumnRight) {
        targetType = ItemType::Ratio;
    } else if (x < resolutionColumnRight) {
        targetType = ItemType::Resolution;
    } else {
        // 设置列的特殊处理
        int settingsY = m_titleHeight + m_separatorHeight;
        for (size_t i = 0; i < m_items.size(); i++) {
            const auto& item = m_items[i];
            if (item.type == ItemType::CaptureWindow ||
                item.type == ItemType::OpenScreenshot ||
                item.type == ItemType::PreviewWindow ||
                item.type == ItemType::Reset ||
                item.type == ItemType::Close) {
                if (y >= settingsY && y < settingsY + m_itemHeight) {
                    return static_cast<int>(i);
                }
                settingsY += m_itemHeight;
            }
        }
        return -1;
    }

    // 处理比例和分辨率列
    int itemY = m_titleHeight + m_separatorHeight;
    for (size_t i = 0; i < m_items.size(); i++) {
        const auto& item = m_items[i];
        if (item.type == targetType) {
            if (y >= itemY && y < itemY + m_itemHeight) {
                return static_cast<int>(i);
            }
            itemY += m_itemHeight;
        }
    }
    
    return -1;
}

void MenuWindow::SetPreviewEnabled(bool enabled) {
    m_previewEnabled = enabled;
    InvalidateRect(m_hwnd, NULL, TRUE);
}
