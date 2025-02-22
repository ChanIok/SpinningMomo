#pragma once
#include "win_config.hpp"
#include <d3d11.h>
#include <winrt/base.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <mutex>
#include <wrl/client.h>
#include "thread_raii.hpp"

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool Initialize(HINSTANCE hInstance);
    bool StartCapture(HWND targetWindow);
    void StopCapture();
    void Cleanup();
    HWND GetHwnd() const { return m_hwnd; }  // 添加getter方法

private:
    static OverlayWindow* instance;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    std::atomic<bool> m_d3dInitialized{false};
    
    bool InitializeD3D();
    bool CreateRenderTarget();
    bool CreateShaderResources();
    void OnFrameArrived();

    bool InitializeCapture();

    // 窗口和Direct3D资源
    HWND m_hwnd = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
    std::mutex m_renderTargetMutex;

    // 捕获相关
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{ nullptr };

        
    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

    // Windows.Graphics.Capture 资源
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{ nullptr };

    // 顶点结构
    struct Vertex {
        float x, y;
        float u, v;
    };

    RECT m_gameWindowRect = {};
    bool m_isFirstShow = true;
    HWND m_gameWindow = nullptr;

    // 添加线程相关成员
    ThreadRAII m_captureThread;
    ThreadRAII m_hookThread;
    ThreadRAII m_positionThread;
    std::atomic<bool> m_running{false};
    POINT m_currentMousePos{0, 0};
    float m_scaleFactor{1.0f};
    HHOOK m_mouseHook{nullptr};

    void CaptureThreadProc();
    void HookThreadProc();
    void PositionUpdateThreadProc();
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);
};
