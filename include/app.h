#pragma once
#include <windows.h>
#include <shellapi.h>
#include "window_capture.h"
#include "gui_manager.h"
#include "config_manager.h"
#include <memory>
#include <string>

// 定义消息和ID
#define ID_HOTKEY_TOGGLE 1
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1000
#define ID_TRAY_TOGGLE 1001
#define ID_TRAY_EXIT 1002

class App {
public:
    App();
    ~App();

    bool Initialize();
    void Run();
    void Cleanup();

    // 单例访问
    static App& GetInstance();

    // 窗口显示控制
    void ToggleVisibility();
    bool IsVisible() const { return isVisible; }

private:
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool CreateMainWindow();
    void ProcessMessages();

    // 托盘相关函数
    bool CreateTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    void ShowTrayMenu();

    // 窗口和设备
    HWND hwnd;
    std::unique_ptr<WindowCapture> windowCapture;
    std::unique_ptr<GuiManager> guiManager;
    
    // 状态标志
    bool running;
    bool isVisible;
    std::wstring targetWindowTitle;

    // 窗口拖动相关
    POINT dragStartPoint;
    RECT dragStartRect;

    // 托盘图标
    NOTIFYICONDATAW nid;

    // 禁止拷贝
    App(const App&) = delete;
    App& operator=(const App&) = delete;
}; 