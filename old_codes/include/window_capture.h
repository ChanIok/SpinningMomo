#pragma once
#include <windows.h>
#include <d3d11.h>
#include <memory>
#include <string>

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

private:
    bool InitializeD3D();
    bool FindTargetWindow(const std::wstring& windowTitle);
    bool CheckStagingTextureSize(UINT width, UINT height);

    HWND targetWindow;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    ID3D11Texture2D* stagingTexture;  // 用于暂存捕获的窗口内容
    
    bool initialized;
}; 