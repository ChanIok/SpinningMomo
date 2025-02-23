#include "overlay_window.hpp"
#include <windowsx.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <d3dcompiler.h>
#include <dwmapi.h>

namespace {
    // 移动着色器代码到匿名命名空间
    const char* vertexShaderCode = R"(
        struct VS_INPUT {
            float2 pos : POSITION;
            float2 tex : TEXCOORD;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos, 0.0f, 1.0f);
            output.tex = input.tex;
            return output;
        }
    )";

    const char* pixelShaderCode = R"(
        Texture2D tex : register(t0);
        SamplerState samp : register(s0);
        float4 main(float4 pos : SV_POSITION, float2 texCoord : TEXCOORD) : SV_Target {
            return tex.Sample(samp, texCoord);
        }
    )";
}

// 添加新的实现
template<typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    Microsoft::WRL::ComPtr<T> result;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
    return result;
}

OverlayWindow* OverlayWindow::instance = nullptr;

OverlayWindow::OverlayWindow() {
    instance = this;
    winrt::init_apartment();
}

OverlayWindow::~OverlayWindow() {
    StopCapture();
    Cleanup();
    instance = nullptr;
}

bool OverlayWindow::Initialize(HINSTANCE hInstance, HWND mainHwnd) {
    // 设置主窗口句柄
    m_mainHwnd = mainHwnd;

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // 创建全屏窗口
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | 
        WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
        L"OverlayWindowClass",
        L"Overlay Window",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!m_hwnd) {
        return false;
    }

    // 设置窗口为完全透明但可见
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    // 初始化 D3D 资源
    if (!InitializeD3D()) {
        return false;
    }
    m_d3dInitialized = true;

    return true;
}

bool OverlayWindow::StartCapture(HWND targetWindow) {
    if (!targetWindow) return false;
    m_gameWindow = targetWindow;

    // 停止现有的捕获
    StopCapture();

    // 计算缩放因子
    RECT gameRect;
    GetWindowRect(targetWindow, &gameRect);
    m_scaleFactor = static_cast<float>(gameRect.right - gameRect.left) / 
                    GetSystemMetrics(SM_CXSCREEN);
    // 启动工作线程
    m_running = true;
    m_renderThreadRunning = true;
    try {
        m_captureThread = ThreadRAII([this]() { CaptureThreadProc(); });
        m_hookThread = ThreadRAII([this]() { HookThreadProc(); });
        m_windowManagerThread = ThreadRAII([this]() { WindowManagerThreadProc(); });
        m_renderThread = ThreadRAII([this]() { RenderThreadProc(); });
        return true;
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("Thread creation failed: " + std::string(e.what()) + "\n").c_str());
        StopCapture();
        return false;
    }
}

void OverlayWindow::CaptureThreadProc() {
    // 初始化 COM
    OutputDebugStringA("CaptureThreadProc0!\n");
    winrt::init_apartment();

    // 检查 D3D 是否已初始化
    if (!m_d3dInitialized) {
        OutputDebugStringA("D3D not initialized\n");
        return;
    }
    OutputDebugStringA("CaptureThreadProc1!\n");
    // 创建捕获资源
    if (!InitializeCapture()) {
        OutputDebugStringA("Failed to initialize capture\n");
        return;
    }
    OutputDebugStringA("CaptureThreadProc2!\n");
    // 使用消息循环保持线程活动
    MSG msg;
    while (m_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void OverlayWindow::HookThreadProc() {
    // 设置鼠标钩子
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, 
                                  GetModuleHandle(NULL), 0);
    if (!m_mouseHook) {
        OutputDebugStringA("Failed to set mouse hook\n");
        return;
    }

    // 获取游戏进程ID
    GetWindowThreadProcessId(m_gameWindow, &m_gameProcessId);
    
    // 设置窗口事件钩子
    m_eventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        m_gameProcessId,  // 只监控游戏进程
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (!m_eventHook) {
        OutputDebugStringA("Failed to set window event hook\n");
    }
    
    MSG msg;
    while (m_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理钩子
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (m_eventHook) {
        UnhookWinEvent(m_eventHook);
        m_eventHook = nullptr;
    }
}

void OverlayWindow::WindowManagerThreadProc() {
    // 创建一个隐藏的窗口来处理定时器消息
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"WindowManagerClass";
    RegisterClassExW(&wc);

    HWND timerWindow = CreateWindowExW(
        0, L"WindowManagerClass", L"Timer Window",
        WS_POPUP, 0, 0, 0, 0, NULL, NULL,
        GetModuleHandle(NULL), NULL
    );

    if (!timerWindow) {
        OutputDebugStringA("Failed to create timer window\n");
        return;
    }

    // 保存窗口句柄供WinEventProc使用
    m_timerWindow = timerWindow;

    // 创建定时器，每10毫秒触发一次
    SetTimer(timerWindow, 1, 10, NULL);

    MSG msg;
    while (m_running && GetMessage(&msg, NULL, 0, 0)) {
        switch (msg.message) {
        case WM_TIMER:
            // 根据鼠标位置更新游戏窗口位置
            if (m_gameWindow) {
                POINT currentPos = m_currentMousePos;
                
                // 计算新的窗口位置
                int gameX = static_cast<int>(currentPos.x * m_scaleFactor);
                int gameY = static_cast<int>(currentPos.y * m_scaleFactor);
                
                int newWindowX = currentPos.x - gameX;
                int newWindowY = currentPos.y - gameY;
                
                SetWindowPos(m_gameWindow, NULL, newWindowX, newWindowY, 
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            break;

        case WM_GAME_WINDOW_FOREGROUND:
            // 确保overlay窗口在游戏窗口上方
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(m_gameWindow, m_hwnd, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    m_timerWindow = nullptr;  // 清除窗口句柄
    KillTimer(timerWindow, 1);
    DestroyWindow(timerWindow);
    UnregisterClassW(L"WindowManagerClass", GetModuleHandle(NULL));
}

void CALLBACK OverlayWindow::WinEventProc(
    HWINEVENTHOOK hook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    if (!instance || !instance->m_running || !instance->m_timerWindow) return;

    // 当游戏窗口被激活时，发送消息到窗口管理线程
    if (event == EVENT_SYSTEM_FOREGROUND) {
        PostMessage(instance->m_timerWindow, WM_GAME_WINDOW_FOREGROUND, 0, 0);
    }
}

LRESULT CALLBACK OverlayWindow::MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && instance) {
        MSLLHOOKSTRUCT* hookStruct = (MSLLHOOKSTRUCT*)lParam;
        instance->m_currentMousePos = hookStruct->pt;
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

bool OverlayWindow::InitializeCapture() {
    // 创建 DirectX 设备
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get IDXGIDevice\n");
        return false;
    }
    // 创建 WinRT 设备
    winrt::com_ptr<::IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create WinRT device\n");
        return false;
    }
    winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    if (!winrtDevice) {
        OutputDebugStringA("Failed to get WinRT device interface\n");
        return false;
    }

    // 创建捕获项
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, 
                                                ::IGraphicsCaptureItemInterop>();
    hr = interop->CreateForWindow(
        m_gameWindow,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(captureItem))
    );
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create capture item\n");
        return false;
    }

    // 获取目标窗口尺寸
    RECT clientRect;
    GetClientRect(m_gameWindow, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    // 创建帧池
    framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { width, height }
    );

    // 设置帧到达回调
    m_frameArrivedToken = framePool.FrameArrived([this](auto&& sender, auto&&) {
        OnFrameArrived();
    });

    // 创建捕获会话
    captureSession = framePool.CreateCaptureSession(captureItem);
    captureSession.IsCursorCaptureEnabled(false);
    
    // 尝试禁用边框
    try {
        auto session3 = captureSession.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>();
        if (session3) {
            session3.IsBorderRequired(false);
        }
    }
    catch (...) {
        // 忽略任何错误，继续执行
    }

    try {
        // 开始捕获
        captureSession.StartCapture();
    }
    catch (...) {
        OutputDebugStringA("Unknown error occurred while starting capture session\n");
        return false;
    }
    OutputDebugStringA("InitializeCapture1!\n");
    // 发送消息给主线程显示窗口
    PostMessage(m_hwnd, WM_SHOW_OVERLAY, 0, 0);
    OutputDebugStringA("InitializeCapture2!\n");
    return true;
}

void OverlayWindow::StopCapture() {
    // 停止标志
    m_running = false;
    m_renderThreadRunning = false;
    m_recreateTextureFlag = true;

    if(framePool){
        framePool.FrameArrived(m_frameArrivedToken);
    }

    // 向线程发送退出消息
    if (m_hookThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_hookThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    if (m_captureThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_captureThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    if (m_windowManagerThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_windowManagerThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    if (m_renderThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_renderThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    OutputDebugStringA("StopCapture0!\n");
    if (m_captureThread.get() && m_captureThread.get()->joinable()) {
        m_captureThread.get()->join();
    }
    OutputDebugStringA("StopCapture0.5!\n");
    if (m_hookThread.get() && m_hookThread.get()->joinable()) {
        m_hookThread.get()->join();
    }
    if (m_windowManagerThread.get() && m_windowManagerThread.get()->joinable()) {
        m_windowManagerThread.get()->join();
    }
    OutputDebugStringA("StopCapture1!\n");
    if (m_renderThread.get() && m_renderThread.get()->joinable()) {
        m_renderThread.get()->join();
    }
    OutputDebugStringA("StopCapture2!\n");

    if (captureSession) {
        captureSession.Close();
        captureSession = nullptr;
    }
    if (framePool) {
        framePool.Close();
        framePool = nullptr;
    }
    captureItem = nullptr;  

    // 清理共享纹理相关资源
    if (m_sharedTexture) {
        m_sharedTexture.Reset();
        m_sharedMutex.Reset();
        m_sharedHandle = nullptr;
    }
    if (m_frameLatencyWaitableObject) {
        CloseHandle(m_frameLatencyWaitableObject);
        m_frameLatencyWaitableObject = nullptr;
    }

    OutputDebugStringA("StopCapture3!\n");
    // 隐藏窗口
    ShowWindow(m_hwnd, SW_HIDE);
}

void OverlayWindow::Cleanup() {
    if (m_frameLatencyWaitableObject) {
        CloseHandle(m_frameLatencyWaitableObject);
        m_frameLatencyWaitableObject = nullptr;
    }
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    // 释放其他 Direct3D 资源
    m_renderTarget.Reset();
    m_swapChain.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_sampler.Reset();
    m_blendState.Reset();
    m_shaderResourceView.Reset();

    // 最后释放 context 和 device
    m_context.Reset();
    m_device.Reset();
}

bool OverlayWindow::InitializeD3D() {
    // 创建设备和交换链
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    
    // 首先创建设备
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context
    );

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create D3D11 device\n");
        return false;
    }

    // 获取DXGI设备
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get DXGI device\n");
        return false;
    }

    // 设置最大帧延迟为3
    hr = dxgiDevice->SetMaximumFrameLatency(3);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to set maximum frame latency\n");
        return false;
    }

    // 获取DXGI Factory6
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory6));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create DXGI Factory6\n");
        return false;
    }

    // 直接获取性能最好的适配器
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter4;
    hr = factory6->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&adapter4)
    );
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get high performance adapter\n");
        return false;
    }

    // 获取适配器信息
    DXGI_ADAPTER_DESC3 desc = {};
    hr = adapter4->GetDesc3(&desc);
    if (SUCCEEDED(hr)) {
        OutputDebugStringW(L"Using graphics adapter: ");
        OutputDebugStringW(desc.Description);
        OutputDebugStringW(L"\n");
    }
    
    BOOL allowTearing = FALSE;
    UINT tearingFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    // 检查可变刷新率支持
    BOOL supportTearing = FALSE;
    if (SUCCEEDED(factory6->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
        &supportTearing,
        sizeof(supportTearing)))) {
        allowTearing = supportTearing;
        if (supportTearing) {
            tearingFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            OutputDebugStringA("Variable refresh rate is supported\n");
        } else {
            OutputDebugStringA("Variable refresh rate is not supported\n");
        }
    }
    
    // 添加帧延迟等待对象标志
    tearingFlags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    // 创建交换链描述
    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = GetSystemMetrics(SM_CXSCREEN);
    scd.Height = GetSystemMetrics(SM_CYSCREEN);
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferCount = 4;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = tearingFlags;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc = {};
    fsDesc.Windowed = TRUE;

    // 创建交换链
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory6->CreateSwapChainForHwnd(
        m_device.Get(),
        m_hwnd,
        &scd,
        &fsDesc,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create swap chain\n");
        return false;
    }

    // 获取 IDXGISwapChain4 接口
    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get IDXGISwapChain4 interface\n");
        return false;
    }

    // 获取帧延迟等待对象
    m_frameLatencyWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
    if (m_frameLatencyWaitableObject) {
        OutputDebugStringA("Frame latency waitable object is supported\n");
    }

    // 禁用Alt+Enter全屏切换
    factory6->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (!CreateRenderTarget()) {
        OutputDebugStringA("Failed to create render target\n");
        return false;
    }

    if (!CreateShaderResources()) {
        OutputDebugStringA("Failed to create shader resources\n");
        return false;
    }

    return true;
}

bool OverlayWindow::CreateRenderTarget() {
    // 获取后缓冲
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;

    // 创建渲染目标视图
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    if (FAILED(hr)) return false;

    return true;
}

bool OverlayWindow::CreateShaderResources() {
    // 编译和创建着色器
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to compile vertex shader\n");
        return false;
    }

    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to compile pixel shader\n");
        return false;
    }

    // 创建着色器
    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create vertex shader\n");
        return false;
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) return false;
    OutputDebugStringA("CreateShaderResources!\n");
    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    
    hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr)) return false;

    // 创建顶点缓冲
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f }
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = m_device->CreateBuffer(&bd, &initData, &m_vertexBuffer);
    if (FAILED(hr)) return false;

    // 创建采样器状态
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_sampler);
    if (FAILED(hr)) return false;
    OutputDebugStringA("CreateSamplerState!\n");
    // 创建混合状态
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) return false;

    return true;
}

void OverlayWindow::OnFrameArrived() {
    if (!m_running) return;
    OutputDebugStringA("OnFrameArrived0!\n");
    if (auto frame = framePool.TryGetNextFrame()) {
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        if (frameTexture) {
            D3D11_TEXTURE2D_DESC desc;
            frameTexture->GetDesc(&desc);

            // 如果尺寸变化，重新创建共享纹理
            if (m_recreateTextureFlag || 
                desc.Width != m_lastTextureDesc.Width || 
                desc.Height != m_lastTextureDesc.Height) {
                CreateSharedTexture(desc.Width, desc.Height);
                m_lastTextureDesc = desc;
                m_recreateTextureFlag = false;
                OutputDebugStringA("OnFrameArrived-change!\n");
            }
            OutputDebugStringA("OnFrameArrived1!\n");
            // 获取互斥锁并复制纹理
            if (m_running && m_sharedTexture && m_sharedMutex) {
                OutputDebugStringA("OnFrameArrived2!\n");
                HRESULT hr = m_sharedMutex->AcquireSync(0, 1000);
                OutputDebugStringA("OnFrameArrived3!\n");
                if (FAILED(hr)) return;
                OutputDebugStringA("OnFrameArrived4!\n");
                m_context->CopyResource(m_sharedTexture.Get(), frameTexture.Get());
                m_context->Flush();
                m_sharedMutex->ReleaseSync(1);
                OutputDebugStringA("OnFrameArrived5!\n");
                // 通知渲染线程
                if (m_renderWindow) {
                    PostMessage(m_renderWindow, WM_NEW_FRAME, 0, 0);
                }
            }
        }
    }
}

void OverlayWindow::RenderFrame() {
    OutputDebugStringA("RenderFrame1!\n");
    if (!m_sharedMutex || !m_renderTarget) return;
    
    // 获取互斥锁
    HRESULT hr = m_sharedMutex->AcquireSync(1, INFINITE);
    OutputDebugStringA("RenderFrame2!\n");
    if (FAILED(hr)) return;
    
    // 等待前一帧完成
    if (m_frameLatencyWaitableObject) {
        WaitForSingleObject(m_frameLatencyWaitableObject, INFINITE);
    }
    
    // 渲染逻辑
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);
    m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);
    
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    viewport.Height = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    
    // 设置渲染状态和资源
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->PSSetShaderResources(0, 1, m_shaderResourceView.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);
    
    // 绘制
    m_context->Draw(4, 0);
    
    // 关闭VSync显示
    m_swapChain->Present(0, 0);
    
    // 释放互斥锁
    m_sharedMutex->ReleaseSync(0);
    OutputDebugStringA("RenderFrame3!\n");
}

bool OverlayWindow::CreateSharedTexture(UINT width, UINT height) {
    if (m_sharedTexture) {
        m_sharedTexture.Reset();
        m_sharedMutex.Reset();
        m_sharedHandle = nullptr;
        m_shaderResourceView.Reset();
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    
    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_sharedTexture);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create shared texture\n");
        return false;
    }
    
    // 获取共享句柄
    Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
    hr = m_sharedTexture.As(&dxgiResource);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get DXGI resource\n");
        return false;
    }
    
    hr = dxgiResource->GetSharedHandle(&m_sharedHandle);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get shared handle\n");
        return false;
    }
    
    // 获取同步互斥量
    hr = m_sharedTexture.As(&m_sharedMutex);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get keyed mutex\n");
        return false;
    }
    
    // 创建SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    hr = m_device->CreateShaderResourceView(
        m_sharedTexture.Get(), 
        &srvDesc, 
        m_shaderResourceView.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create shader resource view\n");
        return false;
    }

    return true;
}

void OverlayWindow::RenderThreadProc() {
    winrt::init_apartment();
    
    // 创建渲染消息窗口
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = RenderWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RenderWindowClass";
    RegisterClassExW(&wc);

    m_renderWindow = CreateWindowExW(
        0, L"RenderWindowClass", L"Render Window",
        WS_POPUP, 0, 0, 0, 0, NULL, NULL,
        GetModuleHandle(NULL), NULL
    );

    if (!m_renderWindow) {
        OutputDebugStringA("Failed to create render window\n");
        return;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    if (m_renderWindow) {
        DestroyWindow(m_renderWindow);
        m_renderWindow = nullptr;
    }
    UnregisterClassW(L"RenderWindowClass", GetModuleHandle(NULL));
}

LRESULT CALLBACK OverlayWindow::RenderWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
            case WM_NEW_FRAME:
                instance->RenderFrame();
                return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
            case WM_DESTROY:
                instance->Cleanup();
                return 0;
            
            // 处理窗口大小调整，保持全屏
            case WM_DISPLAYCHANGE:
                SetWindowPos(hwnd, nullptr, 0, 0,
                    GetSystemMetrics(SM_CXSCREEN),
                    GetSystemMetrics(SM_CYSCREEN),
                    SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;

            case WM_SHOW_OVERLAY:
                ShowWindow(hwnd, SW_SHOWNA);    
                SetWindowPos(hwnd, instance->m_gameWindow, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                SetWindowPos(instance->m_gameWindow, hwnd, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                UpdateWindow(hwnd);
                return 0;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
