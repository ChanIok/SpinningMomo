#pragma once
#include "win_config.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <mutex>
#include <atomic>

class PreviewWindow {
public:
    static constexpr int MIN_WIDTH = 100;           // 最小宽度
    static constexpr int MIN_HEIGHT = 100;          // 最小高度
    static constexpr int TITLE_HEIGHT = 24;         // 标题栏高度

    PreviewWindow();
    ~PreviewWindow();

    bool Initialize(HINSTANCE hInstance);
    void Cleanup();
    bool StartCapture(HWND targetWindow);
    void StopCapture();
    HWND GetHwnd() const { return hwnd; }
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    bool InitializeD3D();
    bool CreateRenderTarget();
    bool CreateShaderResources();
    void OnFrameArrived();

    // 顶点结构
    struct Vertex {
        float x, y;
        float u, v;
    };

    HWND hwnd;
    static PreviewWindow* instance;
    bool m_isFirstShow = true;  // 控制是否是首次显示
    int m_idealSize;            // 理想尺寸（基于屏幕高度计算）

    // D3D资源
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget;

    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    // 捕获相关
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{ nullptr };

    // 窗口拖拽相关
    bool isDragging;
    POINT dragStart;

    // 窗口比例相关
    float m_aspectRatio;  // 当前窗口比例
    RECT m_gameWindowRect = {};         // 游戏窗口的尺寸

    // 互斥锁保护渲染目标访问
    std::mutex renderTargetMutex;
}; 