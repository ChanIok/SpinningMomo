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

class WindowCapturer {
public:
    WindowCapturer();
    ~WindowCapturer();

    // 初始化捕获器
    bool Initialize(HWND hwnd);
    
    // 清理资源
    void Cleanup();

    // 异步捕获截图
    bool CaptureScreenshot(std::function<void(ID3D11Texture2D*)> callback);

    // 控制捕获会话
    void StartCapture();
    void StopCapture();

    void SetCropRegion(const RECT& region) {
        m_cropRegion = region;
        m_needsCropping = true;
    }

private:
    // 确保D3D资源已初始化
    bool EnsureD3DResources();
    
    // 创建捕获会话
    bool CreateCaptureSession();

    void ProcessFrameArrived(ID3D11Texture2D* texture);

    // 窗口句柄
    HWND m_hwnd = nullptr;

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
    bool m_isCapturing = false;

    // 裁剪区域支持
    RECT m_cropRegion{};
    bool m_needsCropping = false;
}; 