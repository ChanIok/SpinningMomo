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
    constexpr UINT ID_EXIT = 2003;               // 退出菜单项ID
    const TCHAR* APP_NAME = TEXT("旋转吧大喵");          
    const TCHAR* WINDOW_CLASS = TEXT("SpinningMomoClass");  
    const TCHAR* CONFIG_FILE = TEXT("config.ini");     // 配置文件名
}

// 窗口旋转器类
class WindowRotator {
public:
    static bool RotateWindow(HWND hwnd) {
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

        // 根据新的宽高比决定是否置顶
        // 如果高度大于宽度，设置为置顶；否则取消置顶
        HWND insertAfter = (newHeight > newWidth) ? HWND_TOPMOST : HWND_NOTOPMOST;

        // 设置新的窗口大小和位置，并根据条件设置置顶状态
        if (!SetWindowPos(hwnd, insertAfter, newLeft, newTop, newWidth, newHeight, 
                         SWP_NOACTIVATE)) {
            return false;
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

// 主应用程序类
class WindowRotatorApp {
public:
    bool Initialize(HINSTANCE hInstance) {
        if (!RegisterWindowClass(hInstance)) return false;
        if (!CreateAppWindow(hInstance)) return false;
        
        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        if (!m_trayIcon->Create()) return false;

        // 注册热键 (Ctrl + Alt + R)
        if (!RegisterHotKey(m_hwnd, Constants::ID_TRAYICON, MOD_CONTROL | MOD_ALT, 'R')) {
            MessageBox(NULL, TEXT("热键注册失败。程序仍可使用，但快捷键将不可用。"),
                      Constants::APP_NAME, MB_ICONWARNING);
        }

        // 显示启动提示
        m_trayIcon->ShowBalloon(Constants::APP_NAME, 
            TEXT("窗口旋转工具已在后台运行。\n按 Ctrl+Alt+R 选择并旋转窗口。"));

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

        // 获取所有窗口
        auto windows = WindowRotator::GetWindows();
        int id = 3000;
        for (const auto& window : windows) {
            InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, id++, 
                      window.second.c_str());
        }

        // 添加分隔线和退出选项
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, Constants::ID_EXIT, 
                  TEXT("退出"));

        // 显示菜单
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                      pt.x, pt.y, 0, m_hwnd, NULL);

        DestroyMenu(hMenu);
        m_windows = std::move(windows);
    }

    void HandleWindowSelect(int id) {
        int index = id - 3000;
        if (index >= 0 && index < m_windows.size()) {
            HWND hwnd = m_windows[index].first;
            if (WindowRotator::RotateWindow(hwnd)) {
                m_trayIcon->ShowBalloon(Constants::APP_NAME, 
                    TEXT("窗口旋转成功！"));
            } else {
                m_trayIcon->ShowBalloon(Constants::APP_NAME, 
                    TEXT("窗口旋转失败。可能需要管理员权限，或窗口不支持调整大小。"));
            }
        }
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

            case WM_USER + 1: {  // Constants::WM_TRAYICON
                if (app && lParam == WM_RBUTTONUP) {
                    app->ShowWindowSelectionMenu();
                }
                return 0;
            }

            case WM_COMMAND: {
                if (!app) return 0;
                WORD cmd = LOWORD(wParam);
                if (cmd >= 3000 && cmd < 4000) {
                    app->HandleWindowSelect(cmd);
                } else if (cmd == Constants::ID_EXIT) {
                    DestroyWindow(hwnd);
                }
                return 0;
            }

            case WM_HOTKEY: {
                if (app && wParam == Constants::ID_TRAYICON) {
                    app->ShowWindowSelectionMenu();
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