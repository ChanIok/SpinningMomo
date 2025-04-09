#pragma once
#include <windows.h>
#include <atomic>
#include "thread_raii.hpp"

// 黑边模式窗口类
// 创建一个全屏黑色背景窗口，用于实现信箱模式（Letterbox Mode）
class LetterboxWindow {
private:
    static LetterboxWindow* instance;  // 静态实例指针，用于回调函数

    HWND m_hwnd;                // 窗口句柄
    HWND m_targetWindow;        // 目标窗口句柄
    HWND m_messageWindow;       // 消息窗口句柄
    HINSTANCE m_hInstance;      // 应用程序实例
    bool m_isVisible;           // 窗口是否可见
    
    // 线程和钩子相关
    ThreadRAII m_eventThread;   // 事件监听线程
    std::atomic<bool> m_running{false};  // 线程运行状态
    HWINEVENTHOOK m_eventHook{nullptr}; // 窗口事件钩子句柄
    DWORD m_targetProcessId{0}; // 目标进程ID

    // 事件监听线程处理函数
    void EventThreadProc();

    // 静态回调函数
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD idEventThread,
        DWORD dwmsEventTime
    );
    static LRESULT CALLBACK MessageWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LetterboxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 启动事件监听线程
    bool StartEventThread();

    // 自定义消息定义
    static const UINT WM_TARGET_WINDOW_FOREGROUND = WM_USER + 100;
    static const UINT WM_HIDE_LETTERBOX = WM_USER + 101;
    static const UINT WM_SHOW_LETTERBOX = WM_USER + 102;
    static const UINT WM_UPDATE_TASKBAR_ZORDER = WM_USER + 103; // 新增：更新任务栏Z序的消息

    // 计时器ID定义
    static const UINT TIMER_TASKBAR_ZORDER = 1001;  // 新增：任务栏Z序计时器ID


public:
    LetterboxWindow();
    ~LetterboxWindow();

    // 初始化黑边窗口
    bool Initialize(HINSTANCE hInstance);
    
    // 显示黑边窗口，并将其置于目标窗口下方
    void Show(HWND targetWindow = nullptr);
    
    // 隐藏黑边窗口（仅改变可见性，不停止线程）
    void Hide();
    
    // 完全关闭黑边窗口功能，停止线程并清理资源
    void Shutdown();
    
    // 更新黑边窗口位置，使其适应目标窗口
    void UpdatePosition(HWND targetWindow = nullptr);
    
    // 检查窗口是否可见
    bool IsVisible() const;
    
    // 检查事件线程是否运行
    bool IsEventThreadRunning() const;
};
