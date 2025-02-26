#include "overlay_window.hpp"
#include <d3dkmthk.h>
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
    Sleep(400);
    // 获取游戏窗口尺寸并计算宽高比
    RECT gameRect;
    GetWindowRect(targetWindow, &gameRect);
    m_cachedGameWidth = gameRect.right - gameRect.left;
    m_cachedGameHeight = gameRect.bottom - gameRect.top;
    double aspectRatio = static_cast<double>(m_cachedGameWidth) / m_cachedGameHeight;

    // 计算适合屏幕的窗口尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (m_cachedGameWidth * screenHeight <= screenWidth * m_cachedGameHeight) {
        // 基于高度计算宽度
        m_windowHeight = screenHeight;
        m_windowWidth = static_cast<int>(screenHeight * aspectRatio);
    } else {
        // 基于宽度计算高度
        m_windowWidth = screenWidth;
        m_windowHeight = static_cast<int>(screenWidth / aspectRatio);
    }
    char buffer[256];
    sprintf_s(buffer, "Resizing swap chain to %dx%d\n", m_windowWidth, m_windowHeight);
    OutputDebugStringA(buffer);

    // 启动工作线程
    m_running = true;
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

    if (!ResizeSwapChain()) {
        OutputDebugStringA("Failed to resize swap chain\n");
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

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 保存窗口句柄供WinEventProc使用
    m_timerWindow = timerWindow;

    // 创建定时器，每16毫秒触发一次
    SetTimer(timerWindow, 1, 16, NULL);

    MSG msg;
    while (m_running && GetMessage(&msg, NULL, 0, 0)) {
        switch (msg.message) {
        case WM_TIMER:
            // 根据鼠标位置更新游戏窗口位置
            if (m_gameWindow) {
                POINT currentPos = m_currentMousePos;

                // 只有当鼠标位置发生变化时才更新窗口位置
                if (currentPos.x != m_lastMousePos.x || currentPos.y != m_lastMousePos.y) {
                    // 计算叠加层窗口的位置
                    int overlayLeft = (screenWidth - m_windowWidth) / 2;
                    int overlayTop = (screenHeight - m_windowHeight) / 2;
                    
                    // 检查鼠标是否在叠加层窗口范围内
                    if (currentPos.x >= overlayLeft && 
                        currentPos.x <= (overlayLeft + m_windowWidth) &&
                        currentPos.y >= overlayTop && 
                        currentPos.y <= (overlayTop + m_windowHeight)) {
                        
                        // 计算鼠标在叠加层窗口中的相对位置（0.0 到 1.0）
                        double relativeX = (currentPos.x - overlayLeft) / (double)m_windowWidth;
                        double relativeY = (currentPos.y - overlayTop) / (double)m_windowHeight;
                        
                        // 使用缓存的游戏窗口尺寸计算新位置
                        int newGameX = static_cast<int>(-relativeX * m_cachedGameWidth + currentPos.x);
                        int newGameY = static_cast<int>(-relativeY * m_cachedGameHeight + currentPos.y);
                        
                        // 更新游戏窗口位置
                        SetWindowPos(m_gameWindow, NULL, newGameX, newGameY, 
                            0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    }
                    
                    // 更新上一次的鼠标位置
                    m_lastMousePos = currentPos;
                }
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
        OutputDebugStringA("EVENT_SYSTEM_FOREGROUND\n");
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
    bool hags_enabled = false; // 替换为 IsHAGSEnabled() 如果实现
    // 设置进程调度优先级
    NTSTATUS status = D3DKMTSetProcessSchedulingPriorityClass(
        GetCurrentProcess(),
        hags_enabled ? D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH : D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
    if (status != 0) {
        char buffer[128];
        sprintf_s(buffer, "Failed to set process priority class: %d\n", status);
        OutputDebugStringA(buffer);
    } else {
        OutputDebugStringA("Process priority class set successfully\n");
    }

    // 设置 GPU 线程优先级
    hr = dxgiDevice->SetGPUThreadPriority(7);
    if (SUCCEEDED(hr)) {
        OutputDebugStringA("GPU priority setup success\n");
    } else {
        char buffer[128];
        sprintf_s(buffer, "Failed to set GPU priority: 0x%08X\n", hr);
        OutputDebugStringA(buffer);
        // 优先级设置失败不影响主要功能，继续执行
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

    // 创建帧池
    framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { m_cachedGameWidth, m_cachedGameHeight }
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
    m_recreateTextureFlag = true;
    m_renderStatesInitialized = false;
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

    if (m_captureThread.get() && m_captureThread.get()->joinable()) {
        m_captureThread.get()->join();
    }
    if (m_hookThread.get() && m_hookThread.get()->joinable()) {
        m_hookThread.get()->join();
    }
    if (m_windowManagerThread.get() && m_windowManagerThread.get()->joinable()) {
        m_windowManagerThread.get()->join();
    }
    if (m_renderThread.get() && m_renderThread.get()->joinable()) {
        m_renderThread.get()->join();
    }

    if (captureSession) {
        captureSession.Close();
        captureSession = nullptr;
    }
    if (framePool) {
        framePool.Close();
        framePool = nullptr;
    }
    captureItem = nullptr;  

    // 清理纹理资源
    {
        std::lock_guard<std::mutex> lock(m_textureMutex);
        m_frameTexture.Reset();
        m_shaderResourceView.Reset();
    }

    if (m_frameLatencyWaitableObject) {
        CloseHandle(m_frameLatencyWaitableObject);
        m_frameLatencyWaitableObject = nullptr;
    }

    // 隐藏窗口
    ShowWindow(m_hwnd, SW_HIDE);

    LONG_PTR currentExStyle = GetWindowLongPtr(instance->m_gameWindow, GWL_EXSTYLE);
    currentExStyle &= ~WS_EX_LAYERED;
    SetWindowLongPtr(instance->m_gameWindow, GWL_EXSTYLE, currentExStyle);
}

void OverlayWindow::Cleanup() {
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
    scd.Width = m_windowWidth > 0 ? m_windowWidth : GetSystemMetrics(SM_CXSCREEN);
    scd.Height = m_windowHeight > 0 ? m_windowHeight : GetSystemMetrics(SM_CYSCREEN);
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferCount = 4;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = tearingFlags;

    // 创建交换链
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory6->CreateSwapChainForHwnd(
        m_device.Get(),
        m_hwnd,
        &scd,
        nullptr,
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

bool OverlayWindow::ResizeSwapChain() {
    if (!m_swapChain) {
        OutputDebugStringA("Swap chain not initialized\n");
        return false;
    }

    // 释放现有的渲染目标视图
    if (m_renderTarget) {
        m_renderTarget.Reset();
    }

    // 获取当前交换链描述
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    HRESULT hr = m_swapChain->GetDesc1(&desc);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get swap chain description\n");
        return false;
    }

    // 调整交换链缓冲区大小
    hr = m_swapChain->ResizeBuffers(
        4,
        m_windowWidth > 0 ? m_windowWidth : GetSystemMetrics(SM_CXSCREEN),
        m_windowHeight > 0 ? m_windowHeight : GetSystemMetrics(SM_CYSCREEN),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        desc.Flags    // 保持原有标志
    );

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to resize swap chain buffers\n");
        return false;
    }

    // 重新创建渲染目标
    if (!CreateRenderTarget()) {
        OutputDebugStringA("Failed to recreate render target after resize\n");
        return false;
    }

    OutputDebugStringA("Swap chain resized successfully\n");
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
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // 双线性过滤
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

bool OverlayWindow::CreateFrameTexture(UINT width, UINT height) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    
    // 释放旧资源
    m_frameTexture.Reset();
    m_shaderResourceView.Reset();

    // 创建新的纹理
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_frameTexture);
    if (FAILED(hr)) return false;

    // 创建着色器资源视图
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_device->CreateShaderResourceView(m_frameTexture.Get(), &srvDesc, &m_shaderResourceView);
    return SUCCEEDED(hr);
}

void OverlayWindow::OnFrameArrived() {
    if (!m_running) return;
    if (!framePool) return;
    
    if (auto frame = framePool.TryGetNextFrame()) {
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        if (!frameTexture) return;

        D3D11_TEXTURE2D_DESC desc;
        frameTexture->GetDesc(&desc);

        // 如果尺寸变化，重新创建纹理
        if (m_recreateTextureFlag || 
            desc.Width != m_lastTextureDesc.Width || 
            desc.Height != m_lastTextureDesc.Height) {
            CreateFrameTexture(desc.Width, desc.Height);
            m_lastTextureDesc = desc;
            m_recreateTextureFlag = false;
            OutputDebugStringA("OnFrameArrived-change!\n");
        }

        // 使用互斥锁保护纹理更新
        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            m_context->CopyResource(m_frameTexture.Get(), frameTexture.Get());
            m_hasNewFrame = true;
        }
        
        // 增加捕获帧计数
        ++m_captureFrameCount;
        
        // 通知渲染线程
        m_frameAvailable.notify_one();
    }
}

void OverlayWindow::PerformRendering() {
    // 清理渲染目标
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);
    m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);

    // 绘制
    m_context->Draw(4, 0);
    
    // 呈现
    m_swapChain->Present(0, 0);

    // 增加渲染帧计数
    ++m_renderFrameCount;
}

void OverlayWindow::UpdateFPS() {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - m_lastFPSUpdateTime).count() / 1000.0f;
    
    if (elapsedTime >= 1.0f) {  // 每秒更新一次
        m_captureFPS = m_captureFrameCount / elapsedTime;
        m_renderFPS = m_renderFrameCount / elapsedTime;
        
        // 输出调试信息
        char debugStr[256];
        sprintf_s(debugStr, "FPS - Capture: %.2f, Render: %.2f\n", 
                 m_captureFPS, m_renderFPS);
        OutputDebugStringA(debugStr);
        
        // 重置计数器和时间
        m_captureFrameCount = 0;
        m_renderFrameCount = 0;
        m_lastFPSUpdateTime = currentTime;
    }
}

void OverlayWindow::RenderFrame() {
    if (!m_frameTexture || !m_renderTarget) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return;
    }

    // 初始化渲染状态（只在第一次调用时执行）
    if (!m_renderStatesInitialized) {
        InitializeRenderStates();
        m_lastFPSUpdateTime = std::chrono::steady_clock::now();  // 初始化FPS计时器
    }

    // 准备帧，检查是否有新帧需要渲染
    if (!PrepareFrame()) {
        return;  // 没有新帧，直接返回
    }

    // 执行渲染
    PerformRendering();

    // 更新FPS
    UpdateFPS();
}

void OverlayWindow::InitializeRenderStates() {
    // 设置固定的渲染状态
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    // 设置视口（使用计算后的窗口尺寸）
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_windowWidth > 0 ? m_windowWidth : GetSystemMetrics(SM_CXSCREEN));
    viewport.Height = static_cast<float>(m_windowHeight > 0 ? m_windowHeight : GetSystemMetrics(SM_CYSCREEN));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // 设置混合状态（只需要设置一次）
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);

    m_renderStatesInitialized = true;
}

bool OverlayWindow::PrepareFrame() {
    std::unique_lock<std::mutex> lock(m_textureMutex);
    if (!m_hasNewFrame) {
        // 等待新帧，带超时
        if (m_frameAvailable.wait_for(lock, std::chrono::milliseconds(16)) 
            == std::cv_status::timeout) {
            return false;
        }
    }

    // 更新着色器资源视图（仅在有新帧时）
    if (m_shaderResourceView) {
        m_context->PSSetShaderResources(0, 1, m_shaderResourceView.GetAddressOf());
    }

    m_hasNewFrame = false;
    return true;
}

void OverlayWindow::RenderThreadProc() {
    // 渲染循环
    while (m_running) {
        RenderFrame();
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
            case WM_DESTROY:
                instance->Cleanup();
                return 0;
            
            // 处理窗口大小调整，保持全屏
            case WM_DISPLAYCHANGE: {
                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                
                // 重新计算窗口尺寸（保持当前比例）
                if (instance->m_windowWidth > 0 && instance->m_windowHeight > 0) {
                    double aspectRatio = static_cast<double>(instance->m_windowWidth) / instance->m_windowHeight;
                    if (instance->m_windowWidth * screenHeight <= screenWidth * instance->m_windowHeight) {
                        instance->m_windowHeight = screenHeight;
                        instance->m_windowWidth = static_cast<int>(screenHeight * aspectRatio);
                    } else {
                        instance->m_windowWidth = screenWidth;
                        instance->m_windowHeight = static_cast<int>(screenWidth / aspectRatio);
                    }
                }
                
                // 计算居中位置
                int left = (screenWidth - instance->m_windowWidth) / 2;
                int top = (screenHeight - instance->m_windowHeight) / 2;
                
                SetWindowPos(hwnd, nullptr, left, top,
                    instance->m_windowWidth, instance->m_windowHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }

            case WM_SHOW_OVERLAY: {
                ShowWindow(hwnd, SW_SHOWNA);
                
                // 计算居中位置
                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                int left = (screenWidth - instance->m_windowWidth) / 2;
                int top = (screenHeight - instance->m_windowHeight) / 2;
                
                // 添加分层窗口样式
                LONG exStyle = GetWindowLong(instance->m_gameWindow, GWL_EXSTYLE);
                SetWindowLong(instance->m_gameWindow, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                
                // 设置透明度 (完全透明，但仍可点击)
                SetLayeredWindowAttributes(instance->m_gameWindow, 0, 1, LWA_ALPHA); // Alpha = 0 表示完全透明

                // 设置窗口位置和大小
                SetWindowPos(hwnd, nullptr,
                    left, top,
                    instance->m_windowWidth, instance->m_windowHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                    
                // 确保游戏窗口在叠加层之下
                SetWindowPos(hwnd, instance->m_gameWindow, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                SetWindowPos(instance->m_gameWindow, hwnd, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    
                UpdateWindow(hwnd);
                return 0;
            }
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
