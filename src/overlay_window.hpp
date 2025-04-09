#pragma once
#include "win_config.hpp"
#include <mutex>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <winrt/base.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <wrl/client.h>
#include "thread_raii.hpp"
#include "win_timer.hpp"

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool Initialize(HINSTANCE hInstance, HWND mainHwnd);
    bool StartCapture(HWND targetWindow, int width = 0, int height = 0);
    void StopCapture(bool hideWindow = true);
    void Cleanup();
    void RestoreGameWindow(bool withDelay = false);
    HWND GetHwnd() const { return m_hwnd; }
    void SetLetterboxMode(bool enabled) { m_useLetterboxMode = enabled; }
    bool IsCapturing() const { return m_running.load(); }
    bool IsVisible() const { return IsWindowVisible(m_hwnd); }

private:
    static OverlayWindow* instance;

    // 自定义消息
    static const UINT WM_GAME_WINDOW_FOREGROUND = WM_USER + 1;
    static const UINT WM_SHOW_OVERLAY = WM_USER + 2;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD idEventThread,
        DWORD dwmsEventTime
    );
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    const int CLEANUP_TIMEOUT = 30000;  // 清理超时（毫秒）

    // 顶点结构
    struct Vertex {
        float x, y;
        float u, v;
    };

    bool m_d3dInitialized = false;
    bool m_renderStatesInitialized = false;

    HWND m_hwnd = nullptr;
    HWND m_mainHwnd = nullptr;
    HWND m_gameWindow = nullptr;
    HWND m_timerWindow = nullptr;
    int m_windowWidth = 0;
    int m_windowHeight = 0;
    int m_screenWidth = 0;
    int m_screenHeight = 0;
    int m_cachedGameWidth = 0;
    int m_cachedGameHeight = 0;
    RECT m_gameWindowRect = {};

    // 控制是否使用黑边填充模式(letterbox/pillarbox)
    bool m_useLetterboxMode = true;

    // Direct3D资源
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
    std::mutex m_renderTargetMutex;

    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    HANDLE m_frameLatencyWaitableObject{nullptr};
    
    // 捕获相关
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{ nullptr };
    winrt::event_token m_frameArrivedToken;
    
    // 纹理资源和同步相关
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_frameTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
    std::mutex m_textureMutex;
    std::condition_variable m_frameAvailable;
    bool m_hasNewFrame{false};
    bool m_recreateTextureFlag{false};
    bool m_createNewSrv = true;

    // 线程相关成员
    ThreadRAII m_captureAndRenderThread;
    ThreadRAII m_hookThread;
    ThreadRAII m_windowManagerThread;
    std::mutex m_captureStateMutex;
    std::atomic<bool> m_running{false};
    WinTimer m_cleanupTimer;  // 清理资源的定时器

    // 钩子相关
    HHOOK m_mouseHook{nullptr};
    HWINEVENTHOOK m_eventHook = nullptr;
    POINT m_currentMousePos{0, 0};
    POINT m_lastMousePos{0, 0};
    DWORD m_gameProcessId = 0;

    bool InitializeD3D();
    bool InitializeCapture();
    void InitializeRenderStates();
    bool ResizeSwapChain();
    bool CreateRenderTarget();
    bool CreateShaderResources();
    void OnFrameArrived();
    void PerformRendering();

    // 线程处理函数
    void CaptureAndRenderThreadProc();
    void HookThreadProc();
    void WindowManagerThreadProc();
};
