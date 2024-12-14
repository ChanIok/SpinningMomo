#pragma once
#include "window_capture.h"
#include "gui_manager.h"
#include <memory>
#include <string>

class App {
public:
    App();
    ~App();

    bool Initialize();
    void Run();
    void Cleanup();

    // 单例访问
    static App& GetInstance();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool CreateMainWindow();
    void ProcessMessages();

    HWND hwnd;
    std::unique_ptr<WindowCapture> windowCapture;
    std::unique_ptr<GuiManager> guiManager;
    
    bool running;
    std::wstring targetWindowTitle;

    // 窗口拖动相关
    POINT dragStartPoint;
    RECT dragStartRect;

    // 禁止拷贝
    App(const App&) = delete;
    App& operator=(const App&) = delete;
}; 