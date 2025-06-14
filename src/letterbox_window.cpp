#include "letterbox_window.hpp"
#include "logger.hpp"
#include <windowsx.h>
#include <dwmapi.h>

// 初始化静态成员
LetterboxWindow* LetterboxWindow::instance = nullptr;

LetterboxWindow::LetterboxWindow()
    : m_hwnd(nullptr), m_targetWindow(nullptr), m_messageWindow(nullptr),
      m_hInstance(nullptr), m_isVisible(false) {
    instance = this;
}

LetterboxWindow::~LetterboxWindow() {
    // 关闭黑边窗口并清理资源
    Shutdown();
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    // 注销窗口类
    UnregisterClass(L"LetterboxWindowClass", m_hInstance);
    
    instance = nullptr;
}

bool LetterboxWindow::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    m_isVisible = false;

    // 注册窗口类
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = LetterboxWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));  // 黑色背景
    wcex.lpszClassName = L"LetterboxWindowClass";
    
    if (!RegisterClassEx(&wcex)) {
        LOG_ERROR("Failed to register letterbox window class");
        return false;
    }

    // 创建窗口，但不立即显示
    m_hwnd = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"LetterboxWindowClass",
        L"Letterbox",
        WS_POPUP,  // 无边框弹出窗口
        0, 0, 0, 0,  // 位置和大小稍后设置
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!m_hwnd) {
        LOG_ERROR("Failed to create letterbox window");
        return false;
    }

    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    LOG_DEBUG("Letterbox window initialized successfully");
    return true;
}

bool LetterboxWindow::StartEventThread() {
    // 如果线程已经在运行，不需要再次启动
    if (m_eventThread.joinable()) {
        return true;
    }
    
    // 启动事件监听线程
    try {
        m_eventThread = std::jthread([this](std::stop_token stoken) { EventThreadProc(stoken); });
        LOG_DEBUG("Letterbox event thread started");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start letterbox event thread: %s", e.what());
        return false;
    }
}

void LetterboxWindow::Show(HWND targetWindow) {
    if (!m_hwnd) return;
    
    // 如果提供了目标窗口，保存它
    if (targetWindow) {
        m_targetWindow = targetWindow;
    }
    
    // 如果没有目标窗口，无法显示
    if (!m_targetWindow) {
        LOG_ERROR("Cannot show letterbox window: no target window");
        return;
    }

    // 如果目标窗口不可见，不显示
    if (!IsWindowVisible(m_targetWindow)) {
        LOG_DEBUG("Target window is not visible, not showing letterbox");
        return;
    }

    // 确保事件监听线程已启动
    if (!IsEventThreadRunning()) {
        if (!StartEventThread()) {
            LOG_ERROR("Failed to start event thread, cannot show letterbox window");
            return;
        }
    }
    
    ShowWindow(m_hwnd, SW_SHOWNA);
    UpdatePosition(m_targetWindow);
    
    m_isVisible = true;
    LOG_DEBUG("Letterbox window shown");
}

void LetterboxWindow::Hide() {
    // 只隐藏窗口，不停止线程
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
        m_isVisible = false;
        LOG_DEBUG("Letterbox window hidden");
    }
}

void LetterboxWindow::Shutdown() {
    // 隐藏窗口
    if (m_isVisible) {
        Hide();
    }
    
    // 停止事件监听线程
    if (m_eventThread.joinable()) {
        m_eventThread.request_stop();
    }

    // 等待线程结束
    if (m_eventThread.joinable()) {
        m_eventThread.join();
    }

    // 清理钩子
    if (m_eventHook) {
        UnhookWinEvent(m_eventHook);
        m_eventHook = nullptr;
    }
    
    LOG_DEBUG("Letterbox window shutdown completed");
}

void LetterboxWindow::UpdatePosition(HWND targetWindow) {
    if (!m_hwnd) return;
    
    // 如果提供了目标窗口，使用它
    if (targetWindow) {
        m_targetWindow = targetWindow;
    }
    
    // 如果没有目标窗口，无法更新位置
    if (!m_targetWindow) {
        LOG_ERROR("Cannot update letterbox position: no target window");
        return;
    }
    
    // 检查目标窗口是否有效
    if (!IsWindow(m_targetWindow)) {
        LOG_ERROR("Target window is no longer valid");
        Hide();
        return;
    }

    // 获取目标窗口的位置和大小
    RECT targetRect;
    GetWindowRect(m_targetWindow, &targetRect);
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 设置黑边窗口为全屏
    SetWindowPos(m_hwnd, m_targetWindow, 0, 0, screenWidth, screenHeight, SWP_NOACTIVATE);
    
    // 创建一个计时器，稍后处理任务栏置底
    SetTimer(m_hwnd, TIMER_TASKBAR_ZORDER, 10, nullptr);
}

bool LetterboxWindow::IsVisible() const {
    return m_isVisible;
}

bool LetterboxWindow::IsEventThreadRunning() const {
    return m_eventThread.joinable();
}

void LetterboxWindow::EventThreadProc(std::stop_token stoken) {
    LOG_DEBUG("Starting letterbox event thread");
    
    // 注册消息窗口类
    WNDCLASSEX wcMessage = {0};
    wcMessage.cbSize = sizeof(WNDCLASSEX);
    wcMessage.lpfnWndProc = MessageWndProc;
    wcMessage.hInstance = m_hInstance;
    wcMessage.lpszClassName = L"SpinningMomoLetterboxMessageClass";
    
    if (!RegisterClassEx(&wcMessage)) {
        LOG_ERROR("Failed to register letterbox message window class");
        return;
    }
    
    // 创建消息窗口
    m_messageWindow = CreateWindowExW(
        0,
        L"SpinningMomoLetterboxMessageClass",
        L"LetterboxMessage",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        HWND_MESSAGE,  // 消息窗口
        nullptr,
        m_hInstance,
        nullptr);
        
    if (!m_messageWindow) {
        LOG_ERROR("Failed to create letterbox message window");
        UnregisterClass(L"LetterboxMessageClass", m_hInstance);
        return;
    }

    // 获取目标窗口进程ID
    GetWindowThreadProcessId(m_targetWindow, &m_targetProcessId);
    
    // 设置窗口事件钩子，监听多种事件
    m_eventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,         // 窗口激活事件
        EVENT_OBJECT_DESTROY,            // 窗口销毁事件
        NULL,
        WinEventProc,
        m_targetProcessId,               // 只监控目标进程
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (!m_eventHook) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to set window event hook. Error code: %d", error);
    }
    
    // 消息循环
    MSG msg;
    while (!stoken.stop_requested() && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理资源
    if (m_eventHook) {
        UnhookWinEvent(m_eventHook);
        m_eventHook = nullptr;
    }
    
    if (m_messageWindow) {
        DestroyWindow(m_messageWindow);
        m_messageWindow = nullptr;
    }
    
    UnregisterClass(L"LetterboxMessageClass", m_hInstance);
    
    LOG_DEBUG("Letterbox event thread stopped");
}

void CALLBACK LetterboxWindow::WinEventProc(
    HWINEVENTHOOK hook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    if (!instance || !instance->m_eventThread.joinable() || !instance->m_messageWindow) return;
    
    // 只处理与目标窗口相关的事件
    if (hwnd == instance->m_targetWindow) {
        switch (event) {
            case EVENT_SYSTEM_FOREGROUND:
                // 窗口被激活，更新黑边窗口位置
                LOG_DEBUG("Target window activated, updating letterbox position");
                PostMessage(instance->m_messageWindow, WM_TARGET_WINDOW_FOREGROUND, 0, 0);
                break;
                
            case EVENT_SYSTEM_MINIMIZESTART:
                // 窗口被最小化，隐藏黑边窗口
                LOG_DEBUG("Target window minimized, hiding letterbox");
                PostMessage(instance->m_messageWindow, WM_HIDE_LETTERBOX, 0, 0);
                break;
                
            case EVENT_OBJECT_DESTROY:
                // 窗口被销毁，隐藏黑边窗口
                LOG_DEBUG("Target window destroyed, hiding letterbox");
                PostMessage(instance->m_messageWindow, WM_HIDE_LETTERBOX, 0, 0);
                break;
        }
    }
}

LRESULT CALLBACK LetterboxWindow::MessageWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!instance) return DefWindowProc(hwnd, message, wParam, lParam);
    
    switch (message) {
        case WM_TARGET_WINDOW_FOREGROUND:
            if (!instance->IsVisible()) {
                instance->Show();
            } else {
                instance->UpdatePosition();
            }
            break;
            
        case WM_HIDE_LETTERBOX:
            // 隐藏黑边窗口
            if (instance->IsVisible()) {
                instance->Hide();
            }
            break;
            
        case WM_SHOW_LETTERBOX:
            // 显示黑边窗口
            if (!instance->IsVisible() && instance->m_targetWindow) {
                instance->Show();
            }
            break;
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}


// 窗口过程函数
LRESULT CALLBACK LetterboxWindow::LetterboxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // 获取当前实例
    LetterboxWindow* pThis = instance;
    if (!pThis) return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message) {
        case WM_TIMER:
            if (wParam == TIMER_TASKBAR_ZORDER) {
                // 将任务栏置于底层
                if (HWND taskbar = FindWindow(TEXT("Shell_TrayWnd"), NULL)) {
                    SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                KillTimer(hwnd, TIMER_TASKBAR_ZORDER);
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            LOG_DEBUG("Mouse button down event received in letterbox window");
            // 点击时激活目标窗口
            if (pThis->m_targetWindow && IsWindow(pThis->m_targetWindow)) {
                LOG_DEBUG("Letterbox clicked, activating target window");
                SetForegroundWindow(pThis->m_targetWindow);
                SetWindowPos(pThis->m_hwnd, pThis->m_targetWindow, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                
                // 将任务栏置于底层
                SetTimer(pThis->m_hwnd, TIMER_TASKBAR_ZORDER, 10, nullptr);
            }
            break;
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}

