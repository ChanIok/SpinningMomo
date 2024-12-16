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

// 显示器信息结构
struct DisplayInfo {
    std::wstring name;        // 显示器名称
    std::wstring deviceName;  // 设备名称（如 "\\\\.\\DISPLAY1"）
    bool isPrimary;           // 是否为主显示器
    bool isPortrait;          // 是否为纵向模式
};

// 常量定义
namespace Constants {
    static const UINT WM_TRAYICON = WM_USER + 1;    // 自定义托盘图标消息
    constexpr UINT ID_TRAYICON = 1;              
    constexpr UINT ID_ROTATE = 2001;             
    constexpr UINT ID_HOTKEY = 2002;             // 修改热键菜单项ID
    constexpr UINT ID_EXIT = 2003;               // 修改为2003
    const TCHAR* APP_NAME = TEXT("旋转吧大喵");          
    const TCHAR* WINDOW_CLASS = TEXT("SpinningMomoClass");  
    const TCHAR* TOOLTIP_FORMAT = TEXT("旋转吧大喵 (%s)"); 
    const TCHAR* CONFIG_FILE = TEXT("config.ini");     // 配置文件名
    const TCHAR* HOTKEY_SECTION = TEXT("Hotkey");      // 热键配置节名
    const TCHAR* HOTKEY_MODIFIERS = TEXT("Modifiers"); // 修饰键配置项
    const TCHAR* HOTKEY_KEY = TEXT("Key");            // 主键配置项
    const TCHAR* DISPLAY_SECTION = TEXT("Display");    // 显示器配置节
    const TCHAR* DISPLAY_DEVICE_NAME = TEXT("DeviceName"); // 显示器设备名称
}

// 屏幕旋转管理类
class ScreenRotator {
public:
    // 获取所有显示器信息
    static std::vector<DisplayInfo> GetDisplays() {
        std::vector<DisplayInfo> displays;
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&displays));
        return displays;
    }

    // 旋转指定显示器
    static bool Rotate(const std::wstring& deviceName, bool toPortrait) {
        DEVMODE dm;
        if (!InitializeDevMode(deviceName, dm)) {
            return false;
        }

        // 保存原始方向并设置新方向
        DWORD originalOrientation = dm.dmDisplayOrientation;
        dm.dmDisplayOrientation = toPortrait ? DMDO_90 : DMDO_DEFAULT;

        // 检查是否需要交换宽高
        bool needSwapDimensions = (toPortrait && originalOrientation == DMDO_DEFAULT) ||
                                (!toPortrait && originalOrientation == DMDO_90);

        // 在需要时交换宽高
        if (needSwapDimensions) {
            std::swap(dm.dmPelsHeight, dm.dmPelsWidth);
        }

        // 设置字段有效标志
        dm.dmFields = DM_DISPLAYORIENTATION;
        if (needSwapDimensions) {
            dm.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT;
        }

        // 应用新的显示设置
        LONG result = ChangeDisplaySettingsEx(
            deviceName.c_str(),
            &dm,
            NULL,
            CDS_UPDATEREGISTRY,
            NULL
        );

        return result == DISP_CHANGE_SUCCESSFUL;
    }

    // 检查指定显示器是否为纵向模式
    static bool IsPortrait(const std::wstring& deviceName) {
        DEVMODE dm;
        if (!InitializeDevMode(deviceName, dm)) {
            return false;
        }
        return dm.dmDisplayOrientation == DMDO_90;
    }

private:
    // 初始化DEVMODE结构体
    static bool InitializeDevMode(const std::wstring& deviceName, DEVMODE& dm) {
        ZeroMemory(&dm, sizeof(dm));
        dm.dmSize = sizeof(dm);
        return EnumDisplaySettings(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &dm);
    }

    // 获取显示器友好名称
    static std::wstring GetDisplayFriendlyName(const wchar_t* deviceName, bool isPrimary) {
        DEVMODE dm;
        if (!InitializeDevMode(deviceName, dm)) {
            return L"未知显示器";
        }

        // 获取实际分辨率（考虑旋转状态）
        int width = dm.dmPelsWidth;
        int height = dm.dmPelsHeight;
        if (dm.dmDisplayOrientation == DMDO_90 || dm.dmDisplayOrientation == DMDO_270) {
            std::swap(width, height);
        }

        // 提取显示器编号
        std::wstring deviceStr = deviceName;
        size_t numPos = deviceStr.find_last_not_of(L"0123456789");
        std::wstring number = deviceStr.substr(numPos + 1);
        
        // 构建名称字符串
        std::wstring name = L"显示器 " + number + L": " + 
                           std::to_wstring(width) + L"×" + std::to_wstring(height);
        
        // 添加主显示器标识
        if (isPrimary) {
            name += L" (主显示器)";
        }
        
        return name;
    }

    // 显示器枚举回调函数
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
        auto displays = reinterpret_cast<std::vector<DisplayInfo>*>(dwData);
        
        MONITORINFOEX monitorInfo = {0};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        if (GetMonitorInfo(hMonitor, &monitorInfo)) {
            DisplayInfo info;
            info.deviceName = monitorInfo.szDevice;
            info.isPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
            info.isPortrait = IsPortrait(info.deviceName);
            info.name = GetDisplayFriendlyName(monitorInfo.szDevice, info.isPrimary);
            displays->push_back(info);
        }
        return TRUE;
    }
};

// 系统托盘图标管理类
class TrayIcon {
public:
    // 构造函数：初始化托盘图标
    TrayIcon(HWND hwnd) : m_hwnd(hwnd) {
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = hwnd;
        m_nid.uID = Constants::ID_TRAYICON;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = Constants::WM_TRAYICON;
        m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        
        // 默认使用主显示器的状态
        bool isPortrait = false;
        auto displays = ScreenRotator::GetDisplays();
        for (const auto& display : displays) {
            if (display.isPrimary) {
                isPortrait = display.isPortrait;
                break;
            }
        }
        UpdateTooltip(isPortrait);
    }

    // 析构函数：清理托盘图标
    ~TrayIcon() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        if (m_nid.hIcon) DestroyIcon(m_nid.hIcon);
    }

    // 创建托盘图标
    bool Create() {
        return Shell_NotifyIcon(NIM_ADD, &m_nid) != FALSE;
    }

    // 更新托盘图标提示文本
    void UpdateTooltip(bool isPortrait) {
        const TCHAR* orientation = isPortrait ? TEXT("纵向模式") : TEXT("横向模式");
        StringCchPrintf(m_nid.szTip, _countof(m_nid.szTip),
                       Constants::TOOLTIP_FORMAT, orientation);
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

    // 显示气泡提示
    void ShowBalloon(const TCHAR* title, const TCHAR* message) {
        m_nid.uFlags = NIF_INFO;
        StringCchCopy(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), title);
        StringCchCopy(m_nid.szInfo, _countof(m_nid.szInfo), message);
        m_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }

private:
    HWND m_hwnd;                  // 窗口句柄
    NOTIFYICONDATA m_nid = {0};   // 托盘图标数据
};

// 主应用程序类
class ScreenRotatorApp {
public:
    ScreenRotatorApp() {
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

        // 获取所有显示器信息
        m_displays = ScreenRotator::GetDisplays();
        
        // 加载配置
        LoadConfig();
    }

    // 析构函数中保存配置
    ~ScreenRotatorApp() {
        SaveConfig();
    }

    // 初始化应用程序
    bool Initialize(HINSTANCE hInstance) {
        // 注册窗口类和建窗口
        if (!RegisterWindowClass(hInstance)) return false;
        if (!CreateAppWindow(hInstance)) return false;
        
        // 创建托盘图标
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) return false;

        // 注册热键
        if (!RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, m_hotkeyModifiers, m_hotkeyKey)) {
            MessageBox(NULL, TEXT("热键注册失败。程序仍可使用，但快捷键将不可用。"),
                      Constants::APP_NAME, MB_ICONWARNING);
        }

        // 显示启动提示
        m_trayIcon->ShowBalloon(Constants::APP_NAME, 
            TEXT("屏幕旋转工具已在后台运行。\n按 Ctrl+Alt+R 切换屏幕方向。"));

        return true;
    }

    // 运行消息循环
    int Run() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return (int)msg.wParam;
    }

    // 切换屏幕方向
    void ToggleOrientation() {
        m_isPortrait = !m_isPortrait;
        if (ScreenRotator::Rotate(m_currentDisplay, m_isPortrait)) {
            m_trayIcon->UpdateTooltip(m_isPortrait);
        } else {
            m_isPortrait = !m_isPortrait; // 失败时恢复状态
            MessageBox(m_hwnd, TEXT("屏幕旋转失败。"), 
                      Constants::APP_NAME, MB_ICONERROR);
        }
    }

    // 设置热键
    void SetHotkey() {
        // 注销现有热键
        UnregisterHotKey(m_hwnd, Constants::ID_TRAYICON);
        
        // 进入热键设置模式
        m_hotkeySettingMode = true;
        
        // 显示提示
        m_trayIcon->ShowBalloon(Constants::APP_NAME, 
            TEXT("请按下新的热键组合...\n支持 Ctrl、Shift、Alt 组合其他按键"));
    }

    // 显示右键菜单
    void ShowContextMenu(POINT pt) {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // 添加显示器子菜单
        HMENU hDisplayMenu = CreatePopupMenu();
        if (hDisplayMenu) {
            int id = 3000; // 显示器菜单项的起始ID
            for (const auto& display : m_displays) {
                UINT flags = MF_BYPOSITION | MF_STRING;
                if (display.deviceName == m_currentDisplay) {
                    flags |= MF_CHECKED;
                }
                InsertMenu(hDisplayMenu, -1, flags, id++, 
                          display.name.c_str());
            }
            InsertMenu(hMenu, 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hDisplayMenu, 
                      TEXT("选择显示器"));
            InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        }

        // 添加其他菜单项
        InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING, Constants::ID_ROTATE,
                  m_isPortrait ? TEXT("切换到横向") : TEXT("切换到纵向"));
        InsertMenu(hMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING, Constants::ID_HOTKEY, 
                  TEXT("修改热键"));
        InsertMenu(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, Constants::ID_EXIT, 
                  TEXT("退出"));

        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hDisplayMenu);
        DestroyMenu(hMenu);
    }

    // 处理显示器选择
    void HandleDisplaySelect(int id) {
        int index = id - 3000;
        if (index >= 0 && index < m_displays.size()) {
            m_currentDisplay = m_displays[index].deviceName;
            m_isPortrait = m_displays[index].isPortrait;
            m_trayIcon->UpdateTooltip(m_isPortrait);
            // 保存显示器配置
            SaveDisplayConfig();
        }
    }

    // 窗口过程函数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // 获取应用程序实例指针
        ScreenRotatorApp* app = reinterpret_cast<ScreenRotatorApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                // 保存应用程序实例指针
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
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
                        app->m_hotkeyKey = static_cast<UINT>(wParam);  // 修复类型转换警告
                        app->m_hotkeySettingMode = false;

                        // 尝试注册新热键
                        if (RegisterHotKey(hwnd, Constants::ID_TRAYICON, modifiers, static_cast<UINT>(wParam))) {
                            app->m_trayIcon->ShowBalloon(Constants::APP_NAME, 
                                TEXT("热键设置成功！"));
                            // 保存新的热键配置
                            app->SaveHotkeyConfig();
                        } else {
                            // 注册失败，恢复默认热键
                            app->m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;
                            app->m_hotkeyKey = 'R';
                            RegisterHotKey(hwnd, Constants::ID_TRAYICON, 
                                        app->m_hotkeyModifiers, app->m_hotkeyKey);
                            app->m_trayIcon->ShowBalloon(Constants::APP_NAME, 
                                TEXT("热键设置失败，已恢复默认热键。"));
                            // 保存默认热键配置
                            app->SaveHotkeyConfig();
                        }
                    }
                }
                return 0;
            }

            case WM_USER + 1: {  // 使用相同的值代替 Constants::WM_TRAYICON
                // 处理托盘图标消息
                if (app && lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    app->ShowContextMenu(pt);
                }
                return 0;
            }

            case WM_COMMAND: {
                // 处理菜单命令
                if (!app) return 0;
                WORD cmd = LOWORD(wParam);
                if (cmd >= 3000 && cmd < 4000) {
                    app->HandleDisplaySelect(cmd);
                } else {
                    switch (cmd) {
                        case Constants::ID_ROTATE:
                            app->ToggleOrientation();
                            break;
                        case Constants::ID_HOTKEY:
                            app->SetHotkey();
                            break;
                        case Constants::ID_EXIT:
                            DestroyWindow(hwnd);
                            break;
                    }
                }
                return 0;
            }

            case WM_HOTKEY: {
                // 处理热键消息
                if (app && wParam == Constants::ID_TRAYICON) {
                    app->ToggleOrientation();
                }
                return 0;
            }

            case WM_DESTROY: {
                // 处理窗口销毁消息
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

private:
    // 注册窗口类
    bool RegisterWindowClass(HINSTANCE hInstance) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = Constants::WINDOW_CLASS;
        return RegisterClassEx(&wc) != 0;
    }

    // 创建应用程序窗口
    bool CreateAppWindow(HINSTANCE hInstance) {
        m_hwnd = CreateWindow(Constants::WINDOW_CLASS, Constants::APP_NAME,
                            WS_POPUP,  // 改用 WS_POPUP 样式
                            0, 0, 0, 0,  // 位置和大小都设为0
                            NULL, NULL, hInstance, this);
        return m_hwnd != NULL;
    }

    HWND m_hwnd = NULL;                      // 窗口句柄
    bool m_isPortrait;                       // 当前是否为纵向模式
    std::unique_ptr<TrayIcon> m_trayIcon;    // 托盘图标管理器
    std::vector<DisplayInfo> m_displays;      // 所有显示器信息
    std::wstring m_currentDisplay;            // 当前选中的显示器
    UINT m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;   // 热键修饰键
    UINT m_hotkeyKey = 'R';                           // 热键主键
    bool m_hotkeySettingMode = false;                  // 是否处于热键设置模式
    std::wstring m_configPath;                         // 配置文件完整路径

    // 加载热键配置
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

    // 保存热键配置
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

    // 使用主显示器
    void UseMainDisplay() {
        for (const auto& display : m_displays) {
            if (display.isPrimary) {
                m_currentDisplay = display.deviceName;
                m_isPortrait = display.isPortrait;
                break;
            }
        }
    }

    // 加载所有配置
    void LoadConfig() {
        LoadHotkeyConfig();
        LoadDisplayConfig();
    }

    // 保存所有配置
    void SaveConfig() {
        SaveHotkeyConfig();
        SaveDisplayConfig();
    }

    // 加载显示器配置
    void LoadDisplayConfig() {
        TCHAR buffer[256];
        if (GetPrivateProfileString(Constants::DISPLAY_SECTION,
                                  Constants::DISPLAY_DEVICE_NAME,
                                  TEXT(""), buffer, _countof(buffer),
                                  m_configPath.c_str()) > 0) {
            // 检查保存的显示器是否存在
            for (const auto& display : m_displays) {
                if (display.deviceName == buffer) {
                    m_currentDisplay = display.deviceName;
                    m_isPortrait = display.isPortrait;
                    return;
                }
            }
        }
        // 如果没有找到保存的显示器，使用主显示器
        UseMainDisplay();
    }

    // 保存显示器配置
    void SaveDisplayConfig() {
        WritePrivateProfileString(Constants::DISPLAY_SECTION,
                                Constants::DISPLAY_DEVICE_NAME,
                                m_currentDisplay.c_str(),
                                m_configPath.c_str());
    }
};

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ScreenRotatorApp app;
    
    // 初始化应用程序
    if (!app.Initialize(hInstance)) {
        MessageBox(NULL, TEXT("应用程序初始化失败"), 
                  Constants::APP_NAME, MB_ICONERROR);
        return 1;
    }

    // 运行消息循环
    return app.Run();
}