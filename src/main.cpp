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


// 常量定义
namespace Constants {
    static const UINT WM_TRAYICON = WM_USER + 1;    // 自定义托盘图标消息
    constexpr UINT ID_TRAYICON = 1;              
    constexpr UINT ID_ROTATE = 2001;             
    constexpr UINT ID_HOTKEY = 2002;             // 修改热键菜单项ID
    constexpr UINT ID_NOTIFY = 2003;             // 提示开关菜单项ID
    constexpr UINT ID_TASKBAR = 2004;            // 任务栏控制菜单项ID
    constexpr UINT ID_EXIT = 2005;               // 退出菜单项ID
    const TCHAR* APP_NAME = TEXT("旋转吧大喵");          
    const TCHAR* WINDOW_CLASS = TEXT("SpinningMomoClass");  
    const TCHAR* CONFIG_FILE = TEXT("config.ini");     // 配置文件名
    const TCHAR* WINDOW_SECTION = TEXT("Window");      // 窗口配置节名
    const TCHAR* WINDOW_TITLE = TEXT("Title");        // 窗口标题配置项
    const TCHAR* HOTKEY_SECTION = TEXT("Hotkey");      // 热键配置节名
    const TCHAR* HOTKEY_MODIFIERS = TEXT("Modifiers"); // 修饰键配置项
    const TCHAR* HOTKEY_KEY = TEXT("Key");            // 主键配置项
    const TCHAR* NOTIFY_SECTION = TEXT("Notify");      // 提示配置节名
    const TCHAR* NOTIFY_ENABLED = TEXT("Enabled");     // 提示开关配置项
    const TCHAR* TASKBAR_SECTION = TEXT("Taskbar");    // 任务栏配置节名
    const TCHAR* TASKBAR_AUTO_HIDE = TEXT("AutoHide"); // 任务栏自动隐藏配置项
}

// 系统托盘图标管理类
class TrayIcon {
public:
    TrayIcon(HWND hwnd) : m_hwnd(hwnd) {
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = hwnd;
        m_nid.uID = Constants::ID_TRAYICON;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = Constants::WM_TRAYICON;
        m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        
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
        m_nid.uFlags = NIF_INFO;
        StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), title);
        StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), message);
        m_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }

private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid = {0};
};

// 窗口旋转器类
class WindowRotator {
public:
    // 添加查找指定标题窗口的方法
    static HWND FindGameWindow() {
        return FindWindow(NULL, TEXT("无限暖暖  "));  // 修正为正确的空格数量（两个空格）
    }

    static std::wstring TrimRight(const std::wstring& str) {
        size_t end = str.find_last_not_of(L' ');
        return (end == std::wstring::npos) ? L"" : str.substr(0, end + 1);
    }

    static bool CompareWindowTitle(const std::wstring& title1, const std::wstring& title2) {
        return TrimRight(title1) == TrimRight(title2);
    }

    static bool RotateWindow(HWND hwnd, bool shouldHideTaskbar) {
        if (!hwnd || !IsWindow(hwnd)) return false;

        // 获取窗口信息
        RECT rect;
        if (!GetWindowRect(hwnd, &rect)) return false;

        // 计算当前窗口大小
        int currentWidth = rect.right - rect.left;
        int currentHeight = rect.bottom - rect.top;

        // 计算原始宽高比
        double aspectRatio = static_cast<double>(currentWidth) / currentHeight;
        
        // 保持高度不变，根据宽高比计算新的宽度
        int newHeight = currentHeight;  // 保持高度不变
        int newWidth = static_cast<int>(newHeight / aspectRatio);

        // 计算新的位置（保持窗口中心不变）
        int centerX = (rect.left + rect.right) / 2;
        int centerY = (rect.top + rect.bottom) / 2;
        int newLeft = centerX - newWidth / 2;
        int newTop = centerY - newHeight / 2;

        // 设置新的窗口大小和位置
        if (!SetWindowPos(hwnd, NULL, newLeft, newTop, newWidth, newHeight, 
                         SWP_NOZORDER | SWP_NOACTIVATE)) {
            return false;
        }

        // 根据配置控制任务栏显示/隐藏
        if (shouldHideTaskbar) {
            HWND hTaskBar = FindWindow(TEXT("Shell_TrayWnd"), NULL);
            if (hTaskBar) {
                if (newHeight > newWidth) {
                    // 竖屏模式，隐藏任务栏
                    ShowWindow(hTaskBar, SW_HIDE);
                } else {
                    // 横屏模式，显示任务栏
                    ShowWindow(hTaskBar, SW_SHOW);
                }
            }
        }

        return true;
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
class WindowRotatorApp {
public:
    WindowRotatorApp() {
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
    }

    ~WindowRotatorApp() {
        SaveConfig();
    }

    bool Initialize(HINSTANCE hInstance) {
        if (!RegisterWindowClass(hInstance)) return false;
        if (!CreateAppWindow(hInstance)) return false;
        
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) return false;

        // 注册热键
        if (!RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, m_hotkeyModifiers, m_hotkeyKey)) {
            ShowNotification(Constants::APP_NAME, 
                TEXT("热键注册失败。程序仍可使用，但快捷键将不可用。"));
        }

        // 显示启动提示
        ShowNotification(Constants::APP_NAME, 
            TEXT("窗口旋转工具已在后台运行。\n按 Ctrl+Alt+R 旋转游戏窗口。"));

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

        // 创建窗口选择子菜单
        HMENU hWindowMenu = CreatePopupMenu();
        if (hWindowMenu) {
            // 获取所有窗口
            auto windows = WindowRotator::GetWindows();
            int id = 3000;
            for (const auto& window : windows) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (WindowRotator::CompareWindowTitle(window.second, m_windowTitle)) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hWindowMenu, -1, flags, id++, 
                          window.second.c_str());
            }

            // 将窗口选择子菜单添加到主菜单
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, 
                      (UINT_PTR)hWindowMenu, TEXT("选择窗口"));

            // 保存窗口列表以供后续使用
            m_windows = std::move(windows);
        }

        // 添加其他菜单项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_HOTKEY, 
                  TEXT("修改热键"));
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_notifyEnabled ? MF_CHECKED : 0), 
                  Constants::ID_NOTIFY, TEXT("显示提示"));
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_taskbarAutoHide ? MF_CHECKED : 0),
                  Constants::ID_TASKBAR, TEXT("自动隐藏任务栏"));
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_EXIT, 
                  TEXT("退出"));

        // 显示菜单
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hMenu);  // 这会自动销毁所有子菜单
    }

    void HandleWindowSelect(int id) {
        int index = id - 3000;
        if (index >= 0 && index < m_windows.size()) {
            m_windowTitle = m_windows[index].second;
            SaveConfig();
            
            // 立即旋转选中的窗口
            if (WindowRotator::RotateWindow(m_windows[index].first, m_taskbarAutoHide)) {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口旋转成功！"), true);  // 成功提示
            } else {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口旋转失败。可能需要管理员权限，或窗口不支持调整大小。"));
            }
        }
    }

    void RotateGameWindow() {
        HWND gameWindow = NULL;
        
        // 如果有保存的窗口标题，先尝试使用它
        if (!m_windowTitle.empty()) {
            auto windows = WindowRotator::GetWindows();
            for (const auto& window : windows) {
                if (WindowRotator::CompareWindowTitle(window.second, m_windowTitle)) {
                    gameWindow = window.first;
                    break;
                }
            }
        }
        
        // 如果没找到保存的窗口，尝试使用默认的游戏窗口
        if (!gameWindow) {
            gameWindow = WindowRotator::FindGameWindow();
        }

        if (gameWindow) {
            if (WindowRotator::RotateWindow(gameWindow, m_taskbarAutoHide)) {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口旋转成功！"), true);  // 成功提示
            } else {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口旋转失败。可能需要管理员权限，或窗口不支持调整大小。"));
            }
        } else {
            ShowNotification(Constants::APP_NAME, 
                TEXT("未找到目标窗口，请确保窗口已启动。"));
        }
    }

    void SetHotkey() {
        // 注销现有热键
        UnregisterHotKey(m_hwnd, Constants::ID_TRAYICON);
        
        // 进入热键设置模式
        m_hotkeySettingMode = true;
        
        // 显示提示
        ShowNotification(Constants::APP_NAME, 
            TEXT("请按下新的热键组合...\n支持 Ctrl、Shift、Alt 组合其他按键"));
    }

    void ShowNotification(const TCHAR* title, const TCHAR* message, bool isSuccess = false) {
        // 如果是成功提示，则根据开关控制；其他提示始终显示
        if (!isSuccess || m_notifyEnabled) {
            m_trayIcon->ShowBalloon(title, message);
        }
    }

    void ToggleNotification() {
        m_notifyEnabled = !m_notifyEnabled;
        SaveNotifyConfig();
    }

    void ToggleTaskbarAutoHide() {
        m_taskbarAutoHide = !m_taskbarAutoHide;
        SaveTaskbarConfig();
    }

    bool IsTaskbarAutoHideEnabled() const {
        return m_taskbarAutoHide;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowRotatorApp* app = reinterpret_cast<WindowRotatorApp*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, 
                               reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return 0;
            }

            case WM_KEYDOWN: {
                // 处理热键设置
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
                            app->ShowNotification(Constants::APP_NAME, 
                                TEXT("热键设置成功！"));
                            // 保存新的热键配置
                            app->SaveHotkeyConfig();
                        } else {
                            // 注册失败，恢复默认热键
                            app->m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;
                            app->m_hotkeyKey = 'R';
                            RegisterHotKey(hwnd, Constants::ID_TRAYICON, 
                                        app->m_hotkeyModifiers, app->m_hotkeyKey);
                            app->ShowNotification(Constants::APP_NAME, 
                                TEXT("热键设置失败，已恢复默认热键。"));
                            // 保存默认热键配置
                            app->SaveHotkeyConfig();
                        }
                    }
                }
                return 0;
            }

            case WM_COMMAND: {
                if (!app) return 0;
                WORD cmd = LOWORD(wParam);
                if (cmd >= 3000 && cmd < 4000) {
                    app->HandleWindowSelect(cmd);
                } else {
                    switch (cmd) {
                        case Constants::ID_HOTKEY:
                            app->SetHotkey();
                            break;
                        case Constants::ID_NOTIFY:
                            app->ToggleNotification();
                            break;
                        case Constants::ID_TASKBAR:
                            app->ToggleTaskbarAutoHide();
                            break;
                        case Constants::ID_EXIT:
                            DestroyWindow(hwnd);
                            break;
                    }
                }
                return 0;
            }

            case WM_USER + 1: {  // Constants::WM_TRAYICON
                if (app && lParam == WM_RBUTTONUP) {
                    app->ShowWindowSelectionMenu();
                }
                return 0;
            }

            case WM_HOTKEY: {
                if (app && wParam == Constants::ID_TRAYICON) {
                    app->RotateGameWindow();
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
    bool m_hotkeySettingMode = false;                  // 是否处于热键设置模式
    bool m_notifyEnabled = false;                      // 是否显示提示，默认关闭
    bool m_taskbarAutoHide = false;                    // 是否自动隐藏任务栏，默认关闭

    void LoadConfig() {
        LoadHotkeyConfig();
        LoadWindowConfig();
        LoadNotifyConfig();
        LoadTaskbarConfig();
    }

    void SaveConfig() {
        SaveHotkeyConfig();
        SaveWindowConfig();
        SaveNotifyConfig();
        SaveTaskbarConfig();
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

    void LoadTaskbarConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::TASKBAR_SECTION,
                                  Constants::TASKBAR_AUTO_HIDE,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_taskbarAutoHide = (_wtoi(buffer) != 0);
        }
    }

    void SaveTaskbarConfig() {
        WritePrivateProfileString(Constants::TASKBAR_SECTION,
                                Constants::TASKBAR_AUTO_HIDE,
                                m_taskbarAutoHide ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WindowRotatorApp app;
    
    if (!app.Initialize(hInstance)) {
        MessageBox(NULL, TEXT("应用程序初始化失败"), 
                  Constants::APP_NAME, MB_ICONERROR);
        return 1;
    }

    return app.Run();
}