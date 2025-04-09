#pragma once
#include "win_config.hpp"
#include "constants.hpp"
#include "win_timer.hpp"
#include <mutex>
#include <atomic>
#include <d3d11.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class PreviewWindow {
public:
    PreviewWindow();
    ~PreviewWindow();

    bool Initialize(HINSTANCE hInstance, HWND mainHwnd);
    void Cleanup();
    bool StartCapture(HWND targetWindow, int width = 0, int height = 0);
    void StopCapture();
    HWND GetHwnd() const { return m_hwnd; }
    
private:
    static constexpr int BASE_TITLE_HEIGHT = 24;    // 基础标题栏高度（96 DPI）
    static constexpr int BASE_FONT_SIZE = 12;       // 基础字体大小（96 DPI）
    static constexpr int BASE_BORDER_WIDTH = 8;     // 基础边框宽度（96 DPI）

    static PreviewWindow* instance;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    const int CLEANUP_TIMEOUT = 30000;  // 清理超时（毫秒）

    // 顶点结构
    struct Vertex {
        float x, y;
        float u, v;
    };

    // 视口框相关
    struct ViewportVertex {
        DirectX::XMFLOAT2 pos;
        DirectX::XMFLOAT4 color;
    };

    // 初始化相关
    bool m_isFirstShow = true;  // 控制是否是首次显示
    bool m_d3dInitialized = false;

    HWND m_hwnd;
    HWND m_mainHwnd = nullptr;
    HWND m_gameWindow = nullptr;
    int m_windowWidth = 0;
    int m_windowHeight = 0;
    RECT m_gameWindowRect = {};

    // D3D资源
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
    std::mutex m_renderTargetMutex;

    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    // 捕获相关
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{ nullptr };
    winrt::event_token m_frameArrivedToken;
    bool m_createNewSrv = true;

    // 视口框相关
    RECT m_viewportRect{};          // 视口框位置和大小
    bool m_viewportVisible = true;  // 是否显示视口框
    bool m_isGameWindowFullyVisible = false;  // 游戏窗口是否完全可见
    POINT m_gameWindowPos{};        // 游戏窗口当前位置

    // 视口框渲染资源
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_viewportVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_viewportVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_viewportPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_viewportInputLayout;

    // 窗口拖拽相关
    bool m_isDragging= false;
    bool m_viewportDragging = false;  // 是否正在拖拽视口
    POINT m_dragStart = {0, 0};
    POINT m_viewportDragStart = {0, 0};  // 拖拽开始时的鼠标位置
    POINT m_viewportDragOffset = {0, 0};  // 拖拽开始时视口相对于鼠标的偏移

    // DPI相关
    UINT m_dpi = 96;                    // 当前DPI值
    int TITLE_HEIGHT = BASE_TITLE_HEIGHT; // 当前DPI下的标题栏高度
    int FONT_SIZE = BASE_FONT_SIZE;     // 当前DPI下的字体大小
    int BORDER_WIDTH = BASE_BORDER_WIDTH; // 当前DPI下的边框宽度

    // 窗口尺寸相关
    float m_aspectRatio;  // 当前窗口比例
    int m_idealSize;     // 当前理想尺寸
    int m_minIdealSize;  // 最小理想尺寸（屏幕短边的1/10）
    int m_maxIdealSize;  // 最大理想尺寸（屏幕长边）

    std::atomic<bool> m_running{false};

    WinTimer m_cleanupTimer;  // 清理资源的定时器

    bool InitializeD3D();
    bool CreateRenderTarget();
    bool CreateShaderResources();
    void OnFrameArrived();
    void UpdateDpiDependentResources();  // 更新DPI相关资源

    // 视口框相关方法
    bool CreateViewportResources();  // 创建视口框渲染资源
    void UpdateViewportRect();       // 更新视口框位置
    void RenderViewport();          // 渲染视口框
}; 