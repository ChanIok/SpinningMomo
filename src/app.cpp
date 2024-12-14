#include "app.h"
#include <stdexcept>

// 全局实例指针
static App* g_AppInstance = nullptr;

// 窗口类名
const wchar_t* WINDOW_CLASS_NAME = L"NikkiLensWindow";

// 声明ImGui的Win32消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    // 初始化窗口捕获
    if (!windowCapture->Initialize()) {
        return false;
    }

    // 创建主窗口
    if (!CreateMainWindow()) {
        return false;
    }

    // 初始化GUI管理器
    if (!guiManager->Initialize(hwnd, windowCapture->GetDevice(), windowCapture->GetContext())) {
        return false;
    }

    // 查找目标窗口
    if (!windowCapture->CaptureWindow(targetWindowTitle)) {
        // 目标窗口未找到，但不阻止程序运行
        OutputDebugStringW(L"Target window not found\n");
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

        // 捕获并显示画面
        ID3D11Texture2D* texture = nullptr;
        if (windowCapture->GetFrame(&texture)) {
            guiManager->Render(texture);
            texture->Release();  // 释放纹理
        } else {
            // 如果获取画面失败，仍然需要渲染GUI
            guiManager->Render(nullptr);
        }

        // 限制帧率
        Sleep(16);  // 约60FPS
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
    // 首先处理ImGui的消息
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

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

    case WM_MOUSEWHEEL:
        if (app->guiManager) {
            float delta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            app->guiManager->OnMouseWheel(delta);
        }
        return 0;

    case WM_LBUTTONDOWN:
        // 左键按下时开始拖动窗口
        SetCapture(hwnd);
        GetCursorPos(&app->dragStartPoint);
        GetWindowRect(hwnd, &app->dragStartRect);
        return 0;

    case WM_MOUSEMOVE:
        // 如果正在拖动窗口
        if (GetCapture() == hwnd)
        {
            POINT currentPoint;
            GetCursorPos(&currentPoint);
            int deltaX = currentPoint.x - app->dragStartPoint.x;
            int deltaY = currentPoint.y - app->dragStartPoint.y;
            
            SetWindowPos(hwnd, nullptr,
                app->dragStartRect.left + deltaX,
                app->dragStartRect.top + deltaY,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        // 释放鼠标捕获
        if (GetCapture() == hwnd)
        {
            ReleaseCapture();
        }
        return 0;
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

    // 设置窗口透明度和像素格式
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);  // 不透明度设为255（完全不透明）

    // 获取窗口大小
    RECT rect;
    GetClientRect(hwnd, &rect);
    guiManager->OnResize(rect.right - rect.left, rect.bottom - rect.top);

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