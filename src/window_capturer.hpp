#pragma once
#include "win_config.hpp"
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.capture.interop.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>
#include "win_timer.hpp"

class WindowCapturer {
public:
    WindowCapturer();
    ~WindowCapturer();
    
    // 清理资源
    void Cleanup();

    // 初始化捕获器
    bool Initialize(HWND hwnd);

    // 添加回调到队列
    void AddCaptureCallback(std::function<void(ID3D11Texture2D*)> callback);

    // 启动和停止捕获
    bool StartCapture();

    // 简化API：设置回调并开始捕获
    bool CaptureOneFrame(HWND hwnd, std::function<void(ID3D11Texture2D*)> callback);

private:
    // 确保D3D资源已初始化
    bool EnsureD3DResources();
    
    // 创建捕获会话
    bool CreateCaptureSession();

    // 检查窗口尺寸是否变化
    bool CheckWindowSizeChanged();

    void ProcessFrameArrived(ID3D11Texture2D* texture);

    HWND m_hwnd = nullptr;
    
    // 目标窗口尺寸
    int m_targetWidth = 0;
    int m_targetHeight = 0;

    // 捕获相关资源
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
    winrt::event_token m_frameArrivedToken;

    // 回调队列
    std::queue<std::function<void(ID3D11Texture2D*)>> m_callbackQueue;
    std::mutex m_queueMutex;

    // 状态标志
    bool m_isInitialized = false;
    std::atomic<bool> m_isCapturing{false};
    bool m_needHideCursor = false;

    WinTimer m_cleanupTimer;
    const int CLEANUP_TIMEOUT = 10000; 
}; 