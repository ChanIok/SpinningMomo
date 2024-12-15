#include "app.h"
#include <memory>
#include "capture_test.h"

// 声明 ImGui 的 Win32 消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 常量定义
constexpr const wchar_t* WINDOW_CLASS_NAME = L"NikkiLensWindowClass";

// 全局实例指针
static App* g_AppInstance = nullptr;

// 前向声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

App::App()
    : hwnd(nullptr)
    , running(true)
    , isVisible(true)
{
    if (g_AppInstance) {
        throw std::runtime_error("Application instance already exists!");
    }
    g_AppInstance = this;
}

App::~App()
{
    Cleanup();
    g_AppInstance = nullptr;
}

App& App::GetInstance()
{
    if (!g_AppInstance) {
        static App instance;
        g_AppInstance = &instance;
    }
    return *g_AppInstance;
}

bool App::Initialize()
{
    if (!CreateMainWindow()) {
        return false;
    }

    // 创建并初始化 GUI 管理器
    guiManager = std::make_unique<GuiManager>();
    if (!guiManager->Initialize(hwnd)) {
        return false;
    }

    // 注册热键
    if (!RegisterHotKey(hwnd, ID_HOTKEY_TOGGLE, MOD_ALT | MOD_SHIFT, 'N')) {
        MessageBoxW(NULL, L"Failed to register hotkey", L"Error", MB_OK | MB_ICONERROR);
        // 继续运行��但热键功能不可用
    }

    // 创建托盘图标
    if (!CreateTrayIcon()) {
        MessageBoxW(NULL, L"Failed to create tray icon", L"Error", MB_OK | MB_ICONERROR);
        // 继续运行，但托盘功能不可用
    }

    // 设置目标窗口句柄
    HWND targetWindow = (HWND)0x000C108E;
    if (!IsWindow(targetWindow)) {
        MessageBoxW(NULL, L"目标窗口不存在", L"错误", MB_OK | MB_ICONERROR);
        return false;
    }

    // 执行截图
    if (!CaptureWindowToFile(targetWindow, L"screenshot.png")) {
        MessageBoxW(NULL, L"截图失败", L"错误", MB_OK | MB_ICONERROR);
        return false;
    }

    // 加载截图到纹理
    if (!guiManager->LoadTextureFromFile(L"screenshot.png")) {
        MessageBoxW(NULL, L"加载截图失败", L"错误", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

bool App::CreateTrayIcon()
{
    ZeroMemory(&nid, sizeof(NOTIFYICONDATAW));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAYICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(GetModuleHandle(nullptr), IDI_APPLICATION);
    lstrcpyW(nid.szTip, L"NikkiLens");

    return Shell_NotifyIconW(NIM_ADD, &nid);
}

void App::RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void App::UpdateTrayIcon()
{
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void App::ShowTrayMenu()
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOGGLE, isVisible ? L"隐藏" : L"显示");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

void App::ToggleVisibility()
{
    isVisible = !isVisible;
    ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    
    // 更新托盘图标提示文本
    lstrcpyW(nid.szTip, isVisible ? L"NikkiLens (显示)" : L"NikkiLens (隐藏)");
    UpdateTrayIcon();
}

bool App::CreateMainWindow()
{
    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ::WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // 创建窗口
    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,    // 扩展样式：总在最前，支持透明
        WINDOW_CLASS_NAME,                 // 窗口类名
        L"NikkiLens",                     // 窗口标题
        WS_POPUP | WS_VISIBLE,            // 窗口样式：无边框，创建时显示
        100, 100,                         // 初始位置
        800, 600,                         // 初始大小
        nullptr,                          // 父窗口
        nullptr,                          // 菜单
        GetModuleHandle(nullptr),         // 实例句柄
        this                              // 传递 this 指针
    );

    if (!hwnd) {
        return false;
    }

    // 设置窗口透明度
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

void App::Run()
{
    while (running) {
        ProcessMessages();
        guiManager->Render();
    }
}

void App::Cleanup()
{
    if (guiManager) {
        guiManager->Cleanup();
        guiManager.reset();
    }

    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

void App::ProcessMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            running = false;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 首先处理 ImGui 的消息
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    App* app = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (app && app->guiManager) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            app->guiManager->OnResize(width, height);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (app && app->guiManager) {
            float delta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            app->guiManager->OnMouseWheel(delta);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (app)
        {
            SetCapture(hwnd);
            GetCursorPos(&app->dragStartPoint);
            GetWindowRect(hwnd, &app->dragStartRect);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (app && GetCapture() == hwnd)
        {
            POINT pt;
            GetCursorPos(&pt);
            int deltaX = pt.x - app->dragStartPoint.x;
            int deltaY = pt.y - app->dragStartPoint.y;
            
            SetWindowPos(hwnd,
                NULL,
                app->dragStartRect.left + deltaX,
                app->dragStartRect.top + deltaY,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        if (GetCapture() == hwnd)
        {
            ReleaseCapture();
        }
        return 0;

    case WM_HOTKEY:
        if (app && wParam == ID_HOTKEY_TOGGLE) {
            app->ToggleVisibility();
            return 0;
        }
        break;

    case WM_TRAYICON:
        if (app)
        {
            if (lParam == WM_RBUTTONUP) {
                app->ShowTrayMenu();
            } else if (lParam == WM_LBUTTONUP) {
                app->ToggleVisibility();
            }
        }
        return 0;

    case WM_COMMAND:
        if (app)
        {
            switch (LOWORD(wParam))
            {
            case ID_TRAY_TOGGLE:
                app->ToggleVisibility();
                return 0;
            case ID_TRAY_EXIT:
                PostQuitMessage(0);
                return 0;
            }
        }
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
} 