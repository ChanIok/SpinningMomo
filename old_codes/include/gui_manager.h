#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/base.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <vector>

// 声明 ImGui 的 Win32 消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    bool Initialize(HWND hwnd);
    void Cleanup();
    void Render();
    bool LoadTextureFromFile(const wchar_t* filename);

    // 窗口事件处理
    void OnResize(UINT width, UINT height);
    void OnMouseWheel(float delta);
    void SetPosition(int x, int y);
    void SetSize(int width, int height);

    // 获取 D3D 设备
    ID3D11Device* GetDevice() const { return d3dDevice.get(); }
    ID3D11DeviceContext* GetContext() const { return d3dContext.get(); }

private:
    bool CreateDeviceD3D();
    bool CreateRenderTarget();
    void CleanupRenderTarget();
    void AdjustWindowForRotation(UINT textureWidth, UINT textureHeight);
    void UpdateWindowSize();

    HWND hwnd;
    bool initialized;

    // D3D11 资源
    winrt::com_ptr<ID3D11Device> d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    winrt::com_ptr<IDXGISwapChain> swapChain;
    winrt::com_ptr<ID3D11RenderTargetView> mainRenderTargetView;
    winrt::com_ptr<ID3D11ShaderResourceView> screenshotTexture;

    // 图像变换
    bool mirrorX;  // 水平镜像
    bool mirrorY;  // 垂直镜像
    float scaleRatio;  // 缩放比例
    int baseTextureWidth;
    int baseTextureHeight;
    int windowWidth;   // 当前窗口宽度
    int windowHeight;  // 当前窗口高度
}; 