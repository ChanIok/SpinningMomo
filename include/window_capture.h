#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
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

private:
    bool InitializeD3D();
    bool FindTargetWindow(const std::wstring& windowTitle);

    HWND targetWindow;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    IDXGIOutputDuplication* deskDupl;
    
    bool initialized;
}; 