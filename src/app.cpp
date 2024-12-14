#include "app.h"
#include <stdexcept>

// 全局实例指针
static App* g_AppInstance = nullptr;

// 窗口类名
const wchar_t* WINDOW_CLASS_NAME = L"NikkiLensWindow";

App::App()
    : hwnd(nullptr)
    , windowCapture(std::make_unique<WindowCapture>())
    , guiManager(std::make_unique<GuiManager>())
    , running(false)
    , targetWindowTitle(L"无限暖暖")
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
        static App instance;  // 使用局部静态变量确保单例
        g_AppInstance = &instance;
    }
    return *g_AppInstance;
}

bool App::Initialize()
{
    if (!CreateMainWindow()) {
        return false;
    }

    if (!windowCapture->Initialize()) {
        return false;
    }

    // 显示窗口
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return true;
}

void App::Run()
{
    running = true;
    while (running) {
        ProcessMessages();
        // 更新窗口
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
}

void App::Cleanup()
{
    if (guiManager) {
        guiManager->Cleanup();
    }
    if (windowCapture) {
        windowCapture->Cleanup();
    }
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    running = false;
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 获取App实例指针
    App* app = g_AppInstance;
    if (!app) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_CREATE:
        return 0;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // 使用深灰色填充窗口背景
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(RGB(64, 64, 64));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (app->guiManager) {
            app->guiManager->OnResize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;

    case WM_NCHITTEST:
        // 允许通过点击窗口任意位置来拖动窗口
        return HTCAPTION;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool App::CreateMainWindow()
{
    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
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
        400, 300,                         // 初始大小
        nullptr,                          // 父窗口
        nullptr,                          // 菜单
        GetModuleHandle(nullptr),         // 实例句柄
        nullptr                           // 额外参数
    );

    if (!hwnd) {
        return false;
    }

    // 设置窗口透明度
    SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);  // 调整透明度为200

    return true;
}

void App::ProcessMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            running = false;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
} 