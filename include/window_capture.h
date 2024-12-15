#pragma once
#include <windows.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

class WindowCapture {
public:
    WindowCapture();
    ~WindowCapture();

    bool Initialize();
    bool CaptureWindow(const std::wstring& windowTitle);
    bool GetFrame(ID3D11Texture2D** texture);
    void Cleanup();

    // 获取D3D设备和上下文
    ID3D11Device* GetDevice() const { return d3dDevice; }
    ID3D11DeviceContext* GetContext() const { return d3dContext; }

    // 帧率控制
    void SetTargetFrameRate(int fps);
    float GetCurrentFrameRate() const { return currentFPS; }

private:
    bool InitializeD3D();
    bool InitializeGraphicsCapture();
    bool FindTargetWindow(const std::wstring& windowTitle);
    bool CheckStagingTextureSize(UINT width, UINT height);
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateD3DDevice(ID3D11Device* device);
    void UpdateFrameStatistics();

    HWND targetWindow;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    ID3D11Texture2D* stagingTexture;  // 用于暂存捕获的窗口内容
    
    // Windows.Graphics.Capture相关
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{ nullptr };
    
    // 帧率控制相关
    int targetFPS;
    float currentFPS;
    LARGE_INTEGER lastFrameTime;
    LARGE_INTEGER frequency;
    int frameCount;
    float frameTimeAccumulator;
    
    bool initialized;
}; 