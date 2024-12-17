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

#define IDI_ICON1 101  // 添加图标ID定义

// 常量定义
namespace Constants {
    static const UINT WM_TRAYICON = WM_USER + 1;    // 自定义托盘图标消息
    constexpr UINT ID_TRAYICON = 1;              
    constexpr UINT ID_ROTATE = 2001;             
    constexpr UINT ID_HOTKEY = 2002;             // 修改热键菜单项ID
    constexpr UINT ID_NOTIFY = 2003;             // 提示开关菜单项ID
    constexpr UINT ID_TASKBAR = 2004;            // 任务栏控制菜单项ID
    constexpr UINT ID_EXIT = 2005;               // 退出菜单项ID
    constexpr UINT ID_RESET = 2006;               // 重置窗口尺寸菜单项ID
    const TCHAR* APP_NAME = TEXT("旋转吧大喵");          
    const TCHAR* WINDOW_CLASS = TEXT("SpinningMomoClass");  
    const TCHAR* CONFIG_FILE = TEXT("config.ini");     // 配置文件名
    const TCHAR* WINDOW_SECTION = TEXT("Window");      // 窗口配置
    const TCHAR* WINDOW_TITLE = TEXT("Title");        // 窗口标题配置项
    const TCHAR* HOTKEY_SECTION = TEXT("Hotkey");      // 热键配置节名
    const TCHAR* HOTKEY_MODIFIERS = TEXT("Modifiers"); // 修饰键配置项
    const TCHAR* HOTKEY_KEY = TEXT("Key");            // 主键配置项
    const TCHAR* NOTIFY_SECTION = TEXT("Notify");      // 提示配置节名
    const TCHAR* NOTIFY_ENABLED = TEXT("Enabled");     // 提示开关配置项
    const TCHAR* TASKBAR_SECTION = TEXT("Taskbar");    // 任务栏配置节名
    const TCHAR* TASKBAR_AUTO_HIDE = TEXT("AutoHide"); // 任务栏自动隐藏配置项
    constexpr UINT ID_RATIO_BASE = 4000;          // 比例菜单项的基础ID
    constexpr UINT ID_RATIO_CUSTOM = 4999;        // 自定义比例菜单项ID
    constexpr UINT ID_QUICK_SELECT = 2007;           // 快捷选择模式菜单项ID
    const TCHAR* QUICK_SELECT_SECTION = TEXT("QuickSelect");  // 快捷选择配置节名
    const TCHAR* QUICK_SELECT_ENABLED = TEXT("Enabled");      // 快捷选择开关配置项
    constexpr UINT ID_WINDOW_BASE = 3000;    // 窗口选择菜单项的基础ID
    constexpr UINT ID_WINDOW_MAX = 3999;     // 窗口选择菜单项的最大ID
    constexpr size_t DEFAULT_RATIO_INDEX = 11;  // 9:16 的索引位置
    const TCHAR* CUSTOM_RATIO_SECTION = TEXT("CustomRatio");  // 自定义比例配置节名
    const TCHAR* CUSTOM_RATIO_LIST = TEXT("RatioList");       // 自定义比例列表配置项
}

// 添加比例结构体定义
struct AspectRatio {
    std::wstring name;     // 比例名称
    double ratio;          // 宽高比值
    
    AspectRatio(const std::wstring& n, double r) 
        : name(n), ratio(r) {}
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

private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid = {0};
};

// 窗口旋转器类
class WindowResizer {
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

    static bool ResizeWindow(HWND hwnd, const AspectRatio& ratio, bool shouldHideTaskbar, 
                            int originalWidth = 0, int originalHeight = 0) {
        if (!hwnd || !IsWindow(hwnd)) return false;

        RECT rect;
        if (!GetWindowRect(hwnd, &rect)) return false;

        int currentWidth = rect.right - rect.left;
        int currentHeight = rect.bottom - rect.top;

        // 如果没有提供原始尺寸，使用当前尺寸
        if (originalWidth == 0 || originalHeight == 0) {
            originalWidth = currentWidth;
            originalHeight = currentHeight;
        }

        int newWidth = originalWidth;
        int newHeight = originalHeight;

        // 固定比例模式
        double originalRatio = static_cast<double>(originalWidth) / originalHeight;
        double targetRatio = ratio.ratio;

        if (targetRatio > originalRatio) {
            // 以宽度为基准
            newWidth = originalWidth;
            newHeight = static_cast<int>(newWidth / targetRatio);
        } else {
            // 以高度为基准
            newHeight = originalHeight;
            newWidth = static_cast<int>(newHeight * targetRatio);
        }

        // 计算屏幕中心位置
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int newLeft = (screenWidth - newWidth) / 2;
        int newTop = (screenHeight - newHeight) / 2;

        // 设置新的窗口大小和位置
        if (!SetWindowPos(hwnd, NULL, newLeft, newTop, newWidth, newHeight, 
                         SWP_NOZORDER | SWP_NOACTIVATE)) {
            return false;
        }

        // 根据配置控制任务栏显示/隐藏
        if (shouldHideTaskbar) {
            HWND hTaskBar = FindWindow(TEXT("Shell_TrayWnd"), NULL);
            if (hTaskBar) {
                ShowWindow(hTaskBar, newHeight > newWidth ? SW_HIDE : SW_SHOW);
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
            ShowNotification(Constants::APP_NAME, 
                TEXT("热键注册失败。程序仍可使用，但快捷键将不可用。"));
        }

        // 显示启动提示
        ShowNotification(Constants::APP_NAME, 
            TEXT("窗口比例调整工具已在后台运行。\n按 Ctrl+Alt+R 调整窗口比例。"));

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

        // 1. 窗口选择（最基础的功能）
        HMENU hWindowMenu = CreatePopupMenu();
        if (hWindowMenu) {
            // 获取所有窗口
            auto windows = WindowResizer::GetWindows();
            int id = Constants::ID_WINDOW_BASE;
            for (const auto& window : windows) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (WindowResizer::CompareWindowTitle(window.second, m_windowTitle)) {
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

        // 2. 比例选择
        HMENU hRatioMenu = CreatePopupMenu();
        if (hRatioMenu) {
            for (size_t i = 0; i < m_ratios.size(); ++i) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (i == m_currentRatioIndex) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hRatioMenu, -1, flags,
                          Constants::ID_RATIO_BASE + i, m_ratios[i].name.c_str());
            }
            
            // 添加分隔线和自定义比例选项
            InsertMenu(hRatioMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenu(hRatioMenu, -1, MF_BYPOSITION | MF_STRING,
                      Constants::ID_RATIO_CUSTOM, TEXT("添加自定义比例..."));
                      
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                      (UINT_PTR)hRatioMenu, TEXT("窗口比例"));
        }

        // 3. 重置选项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
                  Constants::ID_RESET, TEXT("重置窗口尺寸"));

        // 第一个分隔线
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 4. 操作模式设置
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_quickSelectEnabled ? MF_CHECKED : 0),
                  Constants::ID_QUICK_SELECT, TEXT("快捷选择模式"));
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
                  Constants::ID_HOTKEY, TEXT("修改热键"));

        // 第二个分隔线
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 5. 其他设置
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_notifyEnabled ? MF_CHECKED : 0), 
                  Constants::ID_NOTIFY, TEXT("显示旋转提示"));
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | (m_taskbarAutoHide ? MF_CHECKED : 0),
                  Constants::ID_TASKBAR, TEXT("自动隐藏任务栏"));

        // 最后的分隔线
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

        // 6. 退出选项（永远放在最后）
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, 
                  Constants::ID_EXIT, TEXT("退出"));

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

            // 获取新选择窗口的原始尺寸
            HWND hwnd = m_windows[index].first;
            RECT rect;
            if (GetWindowRect(hwnd, &rect)) {
                m_originalWidth = rect.right - rect.left;
                m_originalHeight = rect.bottom - rect.top;
                m_windowModified = false;  // 重置修改状态
                ShowNotification(Constants::APP_NAME, 
                        TEXT("已选择窗口并重置窗口尺寸记录"));
            }
            
        }
    }

    void HandleRatioSelect(UINT id) {
        if (id == Constants::ID_RATIO_CUSTOM) {
            // 打开配置文件
            ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), 
                        m_configPath.c_str(), NULL, SW_SHOW);
            
            // 显示提示
            ShowNotification(Constants::APP_NAME, 
                TEXT("添加自定义比例步骤：\n"
                     "1. 找到 [CustomRatio] 节\n"
                     "2. 在 RatioList 后添加比例\n"
                     "3. 保存并重启软件"));
            return;
        }

        size_t index = id - Constants::ID_RATIO_BASE;
        if (index < m_ratios.size()) {
            m_currentRatioIndex = index;
            
            HWND gameWindow = NULL;
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
                // 初始化原始尺寸
                InitializeOriginalSize(gameWindow);

                if (WindowResizer::ResizeWindow(gameWindow, m_ratios[m_currentRatioIndex], 
                                              m_taskbarAutoHide, m_originalWidth, m_originalHeight)) {
                    m_windowModified = true;
                    ShowNotification(Constants::APP_NAME, 
                        TEXT("窗口比例调整成功！"), true);
                } else {
                    ShowNotification(Constants::APP_NAME, 
                        TEXT("窗口比例调整失败。可能需要管理员权限，或窗口不支持调整大小。"));
                }
            } else {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("未找到目标窗口，请确保窗口已启动。"));
            }
        }
    }

    void ResizeGameWindow() {
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
            // 初始化原始尺寸
            InitializeOriginalSize(gameWindow);

            // 如果窗口已被修改，先重置窗口
            if (m_windowModified) {
                AspectRatio resetRatio(TEXT("重置"), 
                                     static_cast<double>(m_originalWidth) / m_originalHeight);
                if (WindowResizer::ResizeWindow(gameWindow, resetRatio, m_taskbarAutoHide, 
                                              m_originalWidth, m_originalHeight)) {
                    m_windowModified = false;
                    ShowNotification(Constants::APP_NAME, 
                        TEXT("窗口已重置为原始尺寸。"), true);
                    return;
                }
            }

            // 应用选择的比例
            if (WindowResizer::ResizeWindow(gameWindow, m_ratios[m_currentRatioIndex], 
                                          m_taskbarAutoHide, m_originalWidth, m_originalHeight)) {
                m_windowModified = true;
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口比例调整成功！"), true);
            } else {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口比例调整失败。可能需要管理员权限，或窗口不支持调整大小。"));
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
                if (cmd >= Constants::ID_RATIO_BASE && cmd <= Constants::ID_RATIO_CUSTOM) {
                    app->HandleRatioSelect(cmd);
                } else if (cmd >= Constants::ID_WINDOW_BASE && cmd <= Constants::ID_WINDOW_MAX) {
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
                        case Constants::ID_RESET:
                            app->ResetWindowSize();
                            break;
                        case Constants::ID_QUICK_SELECT:
                            app->ToggleQuickSelect();
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
                    if (app->IsQuickSelectEnabled()) {
                        POINT pt;
                        GetCursorPos(&pt);
                        app->ShowRatioSelectionMenu(pt);
                    } else {
                        app->ResizeGameWindow();
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
    bool m_hotkeySettingMode = false;                  // 是否处于热键设置模式
    bool m_notifyEnabled = false;                      // 是否显示提示，默认关闭
    bool m_taskbarAutoHide = false;                    // 是否自动隐藏任务栏，默认关闭
    std::vector<AspectRatio> m_ratios;
    size_t m_currentRatioIndex = Constants::DEFAULT_RATIO_INDEX;  // 使用常量初始化
    int m_originalWidth = 0;   // 原始窗口宽度
    int m_originalHeight = 0;  // 原始窗口高度
    bool m_windowModified = false;  // 窗口是否被修改过
    bool m_quickSelectEnabled = false;  // 快捷选择模式是否启用

    void InitializeRatios() {
        // 横屏比例（从宽到窄）
        m_ratios.emplace_back(TEXT("32:9"), 32.0/9.0);
        m_ratios.emplace_back(TEXT("21:9"), 21.0/9.0);
        m_ratios.emplace_back(TEXT("16:9"), 16.0/9.0);
        m_ratios.emplace_back(TEXT("16:10"), 16.0/10.0);
        m_ratios.emplace_back(TEXT("5:4"), 5.0/4.0);
        m_ratios.emplace_back(TEXT("4:3"), 4.0/3.0);
        m_ratios.emplace_back(TEXT("3:2"), 3.0/2.0);
        m_ratios.emplace_back(TEXT("1:1"), 1.0/1.0);
        
        // 竖屏比例（从窄到宽）
        m_ratios.emplace_back(TEXT("2:3"), 2.0/3.0);
        m_ratios.emplace_back(TEXT("3:4"), 3.0/4.0);
        m_ratios.emplace_back(TEXT("4:5"), 4.0/5.0);
        m_ratios.emplace_back(TEXT("9:16"), 9.0/16.0);

        // 加载自定义比例
        LoadCustomRatios();
    }

    void LoadConfig() {
        // 检查配置文件是否存在
        if (GetFileAttributes(m_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            // 创建默认配置文件
            WritePrivateProfileString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, TEXT(""), m_configPath.c_str());
            WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, TEXT("3"), m_configPath.c_str());
            WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, TEXT("82"), m_configPath.c_str());
            WritePrivateProfileString(Constants::NOTIFY_SECTION, Constants::NOTIFY_ENABLED, TEXT("0"), m_configPath.c_str());
            WritePrivateProfileString(Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTO_HIDE, TEXT("0"), m_configPath.c_str());
            WritePrivateProfileString(Constants::QUICK_SELECT_SECTION, Constants::QUICK_SELECT_ENABLED, TEXT("0"), m_configPath.c_str());
            WritePrivateProfileString(Constants::CUSTOM_RATIO_SECTION, Constants::CUSTOM_RATIO_LIST, TEXT(""), m_configPath.c_str());
        }

        // 加载各项配置
        LoadHotkeyConfig();
        LoadWindowConfig();
        LoadNotifyConfig();
        LoadTaskbarConfig();
        LoadQuickSelectConfig();
    }

    void SaveConfig() {
        SaveHotkeyConfig();
        SaveWindowConfig();
        SaveNotifyConfig();
        SaveTaskbarConfig();
        SaveQuickSelectConfig();
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

    void ResetWindowSize() {
        if (m_originalWidth == 0 || m_originalHeight == 0) {
            ShowNotification(Constants::APP_NAME, 
                TEXT("没有保存的原始窗口尺寸。"));
            return;
        }

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
            // 创建一个特殊的比例对象用于重置
            AspectRatio resetRatio(TEXT("重置"), 
                                 static_cast<double>(m_originalWidth) / m_originalHeight);
            
            if (WindowResizer::ResizeWindow(gameWindow, resetRatio, m_taskbarAutoHide, m_originalWidth, m_originalHeight)) {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("窗口已重置为原始尺寸。"), true);
            } else {
                ShowNotification(Constants::APP_NAME, 
                    TEXT("重置窗口尺寸失败。"));
            }
        } else {
            ShowNotification(Constants::APP_NAME, 
                TEXT("未找到目标窗口，请确保窗口已启动。"));
        }
    }

    void ShowRatioSelectionMenu(const POINT& pt) {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // 添加比例选项
        for (size_t i = 0; i < m_ratios.size(); ++i) {
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == m_currentRatioIndex) {
                flags |= MF_CHECKED;
            }
            InsertMenu(hMenu, -1, flags,
                      Constants::ID_RATIO_BASE + i, m_ratios[i].name.c_str());
        }

        // 显示菜单
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hMenu);
    }

    void ToggleQuickSelect() {
        m_quickSelectEnabled = !m_quickSelectEnabled;
        SaveQuickSelectConfig();
    }

    void LoadQuickSelectConfig() {
        TCHAR buffer[32];
        if (GetPrivateProfileString(Constants::QUICK_SELECT_SECTION,
                                  Constants::QUICK_SELECT_ENABLED,
                                  TEXT("0"), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            m_quickSelectEnabled = (_wtoi(buffer) != 0);
        }
    }

    void SaveQuickSelectConfig() {
        WritePrivateProfileString(Constants::QUICK_SELECT_SECTION,
                                Constants::QUICK_SELECT_ENABLED,
                                m_quickSelectEnabled ? TEXT("1") : TEXT("0"),
                                m_configPath.c_str());
    }

    // 新增方法：初始化原始窗口尺寸
    void InitializeOriginalSize(HWND gameWindow) {
        if (!m_windowModified) {  // 只在窗口未被修改时初始化
            RECT rect;
            if (GetWindowRect(gameWindow, &rect)) {
                m_originalWidth = rect.right - rect.left;
                m_originalHeight = rect.bottom - rect.top;
            }
        }
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
                        errorDetails += ratio + TEXT("\n");
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
                    // 使用 MessageBox 而不是通知来显示错误
                    std::wstring errorMsg = TEXT("以下自定义比例格式错误：\n\n") + 
                                          errorDetails + TEXT("\n\n") +
                                          TEXT("请确保：\n") +
                                          TEXT("1. 使用英文冒号 \":\" 分隔数字\n") +
                                          TEXT("2. 使用英文逗号 \",\" 分隔多个比例\n") +
                                          TEXT("3. 只输入数字，例如：16:10,17:10");
                    
                    MessageBox(NULL, errorMsg.c_str(), Constants::APP_NAME, 
                              MB_ICONWARNING | MB_OK);
                }
            }
        } catch (...) {
            // 如果发生任何错误，显示一个通用错误消息
            MessageBox(NULL, 
                TEXT("加载自定义比例时发生错误。\n请检查配置文件格式是否正确。"), 
                Constants::APP_NAME, MB_ICONERROR | MB_OK);
        }
    }

public:
    bool IsQuickSelectEnabled() const {
        return m_quickSelectEnabled;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WindowResizerApp app;
    
    if (!app.Initialize(hInstance)) {
        MessageBox(NULL, TEXT("应用程序初始化失败"), 
                  Constants::APP_NAME, MB_ICONERROR);
        return 1;
    }

    return app.Run();
}