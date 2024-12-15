#pragma once
#include <windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "window_capture.h"

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, WindowCapture* capture);
    void Render(ID3D11Texture2D* texture);
    void Cleanup();

    // 窗口事件处理
    void OnResize(UINT width, UINT height);
    void SetPosition(int x, int y);
    void SetSize(int width, int height);
    void OnMouseWheel(float delta);

private:
    bool CreateRenderTarget();
    void CleanupRenderTarget();
    ID3D11ShaderResourceView* CreateTextureView(ID3D11Texture2D* texture);
    void AdjustWindowForRotation(UINT textureWidth, UINT textureHeight);
    void UpdateWindowSize();

    HWND hwnd;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    ID3D11RenderTargetView* mainRenderTargetView;
    IDXGISwapChain* swapChain;
    
    // 纹理相关
    ID3D11ShaderResourceView* textureView;
    bool mirrorX;  // 水平镜像
    bool mirrorY;  // 垂直镜像
    
    int windowWidth;
    int windowHeight;
    float scaleRatio;
    int baseTextureWidth;
    int baseTextureHeight;
    bool initialized;
    WindowCapture* windowCapture;
}; 