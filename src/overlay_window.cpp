#include "overlay_window.hpp"
#include <d3dkmthk.h>
#include <windowsx.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <d3dcompiler.h>
#include <dwmapi.h>
#include "logger.hpp"

namespace {
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
    if (m_cleanupTimer.IsRunning()) {
        m_cleanupTimer.Cancel();
    }
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
        DWORD error = GetLastError();
        LOG_ERROR("Failed to register overlay window class. Error code: %d", error);
        return false;
    }

    // 创建全屏窗口
    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | 
        WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
        L"OverlayWindowClass",
        L"Overlay Window",
        WS_POPUP,
        0, 0, m_screenWidth, m_screenHeight,
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!m_hwnd) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to create overlay window. Error code: %d", error);
        return false;
    }

    // 设置窗口为完全透明但可见
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    return true;
}

bool OverlayWindow::StartCapture(HWND targetWindow, int width, int height) {
    if (m_cleanupTimer.IsRunning()) {
        LOG_DEBUG("Canceling cleanup timer due to new capture");
        m_cleanupTimer.Cancel();
    }

    if (!targetWindow) return false;

    m_gameWindow = targetWindow;

    // 获取游戏窗口尺寸并计算宽高比
    if (width <= 0 || height <= 0) {
        // 如果未提供有效的宽高，则通过GetWindowRect获取
        RECT gameRect;
        GetWindowRect(targetWindow, &gameRect);
        m_cachedGameWidth = gameRect.right - gameRect.left;
        m_cachedGameHeight = gameRect.bottom - gameRect.top;
    } else {
        // 使用传入的宽高值
        m_cachedGameWidth = width;
        m_cachedGameHeight = height;
    }

    LOG_DEBUG("Game window dimensions: %dx%d", m_cachedGameWidth, m_cachedGameHeight);

    double aspectRatio = static_cast<double>(m_cachedGameWidth) / m_cachedGameHeight;

    // 如果宽度和高度都小于屏幕尺寸，则不需要启动
    if (m_cachedGameWidth <= m_screenWidth && m_cachedGameHeight <= m_screenHeight) {
        LOG_DEBUG("Game window fits within screen dimensions, no need for overlay");
        RestoreGameWindow();
        return true;
    }
    
    // 初始化 D3D 资源
    if (!m_d3dInitialized) {
        if (!InitializeD3D()) {
            LOG_ERROR("Failed to initialize Direct3D resources");
            return false;
        }
        m_d3dInitialized = true;
    }

    if (m_cachedGameWidth * m_screenHeight <= m_screenWidth * m_cachedGameHeight) {
        // 基于高度计算宽度
        m_windowHeight = m_screenHeight;
        m_windowWidth = static_cast<int>(m_screenHeight * aspectRatio);
    } else {
        // 基于宽度计算高度
        m_windowWidth = m_screenWidth;
        m_windowHeight = static_cast<int>(m_screenWidth / aspectRatio);
    }

    LOG_DEBUG("Overlay window dimensions: %dx%d", m_windowWidth, m_windowHeight);

    // 启动工作线程
    m_running.store(true);
    try {
        m_captureAndRenderThread = ThreadRAII([this]() { CaptureAndRenderThreadProc(); });
        m_hookThread = ThreadRAII([this]() { HookThreadProc(); });
        m_windowManagerThread = ThreadRAII([this]() { WindowManagerThreadProc(); });
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Thread creation failed: %s", e.what());
        StopCapture();
        return false;
    }
}

// 线程处理函数
void OverlayWindow::CaptureAndRenderThreadProc() {
    LOG_DEBUG("Starting capture and render thread");
    winrt::init_apartment();

    // 延迟防止闪烁
    if(IsWindowVisible(m_hwnd)){
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!m_running) {
        LOG_DEBUG("Exiting capture and render thread");
        return;
    }
    
    if (!ResizeSwapChain()) {
        LOG_ERROR("Failed to resize swap chain");
        return;
    }
    
    InitializeRenderStates();

    // 创建捕获资源
    if (!InitializeCapture()) {
        LOG_ERROR("Failed to initialize screen capture resources");
        return;
    }

    PostMessage(m_hwnd, WM_SHOW_OVERLAY, 0, 0);

    // 消息处理和渲染循环
    MSG msg;
    while (m_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    LOG_DEBUG("Exiting capture and render thread");
}

void OverlayWindow::HookThreadProc() {
    // 设置鼠标钩子
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, 
                                  GetModuleHandle(NULL), 0);
    if (!m_mouseHook) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to set mouse hook. Error code: %d", error);
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
        DWORD error = GetLastError();
        LOG_ERROR("Failed to set window event hook. Error code: %d", error);
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
        DWORD error = GetLastError();
        LOG_ERROR("Failed to create timer window. Error code: %d", error);
        return;
    }

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
                    int overlayLeft = (m_screenWidth - m_windowWidth) / 2;
                    int overlayTop = (m_screenHeight - m_windowHeight) / 2;
                    
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
        LOG_DEBUG("Detected foreground window change. Window handle: 0x%p", hwnd);
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
    LOG_DEBUG("Initializing screen capture");

    // 创建 DirectX 设备
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IDXGIDevice, HRESULT: 0x%08X", hr);
        return false;
    }

    // 设置进程调度优先级
    bool hags_enabled = false; // 替换为 IsHAGSEnabled() 如果实现
    NTSTATUS status = D3DKMTSetProcessSchedulingPriorityClass(
        GetCurrentProcess(),
        hags_enabled ? D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH : D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
    if (status != 0) {
        // 可能是 Windows 10 2004 之前的版本，忽略错误
        LOG_ERROR("Failed to set process priority class. Status code: %d", status);
    } else {
        LOG_INFO("Process priority class set successfully");
    }

    // 设置 GPU 线程优先级，可能没实际效果
    hr = dxgiDevice->SetGPUThreadPriority(7);
    if (SUCCEEDED(hr)) {
        LOG_DEBUG("GPU thread priority setup successful");
    } else {
        LOG_ERROR("Failed to set GPU thread priority. HRESULT: 0x%08X", hr);
        // 优先级设置失败不影响主要功能，继续执行
    }

    // 创建 WinRT 设备
    winrt::com_ptr<::IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create WinRT device. HRESULT: 0x%08X", hr);
        return false;
    }
    m_winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    if (!m_winrtDevice) {
        LOG_ERROR("Failed to get WinRT device interface");
        return false;
    }

    // 创建捕获项
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, 
                                                ::IGraphicsCaptureItemInterop>();
    hr = interop->CreateForWindow(
        m_gameWindow,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(m_captureItem))
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create capture item. HRESULT: 0x%08X", hr);
        return false;
    }

    // 创建帧池
    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { m_cachedGameWidth, m_cachedGameHeight }
    );

    // 设置帧到达回调
    m_frameArrivedToken = m_framePool.FrameArrived([this](auto&& sender, auto&&) {
        OnFrameArrived();
    });

    // 创建捕获会话
    m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);


    // 安全地尝试设置 IsCursorCaptureEnabled
    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
        L"IsCursorCaptureEnabled")) 
    {
        // Windows 10 2004 (Build 19041)
        m_captureSession.IsCursorCaptureEnabled(false);  // 禁用鼠标捕获
    } else {
        LOG_INFO("Cursor capture setting not available on this Windows version");
    }
    
    // 尝试禁用边框 - 使用ApiInformation检查API是否可用
    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
        L"IsBorderRequired")) 
    {
        // Windows 10 2104 (Build 20348)
        m_captureSession.IsBorderRequired(false);
    } else {
        LOG_INFO("Border requirement setting not available on this Windows version");
    }

    try {
        // 开始捕获
        m_captureSession.StartCapture();
    }
    catch (const winrt::hresult_error& e) {
        LOG_ERROR("WinRT error occurred while starting capture session: %s", winrt::to_string(e.message()).c_str());
        return false;
    }
    catch (...) {
        LOG_ERROR("Unknown error occurred while starting capture session");
        return false;
    }

    LOG_DEBUG("Screen capture initialization completed successfully");
    return true;
}

void OverlayWindow::StopCapture(bool hideWindow) {
    if (!m_running.load()) {
        return;
    }
    // 停止标志
    m_running.store(false);

    if(m_framePool){
        m_framePool.FrameArrived(m_frameArrivedToken);
    }

    // 向线程发送退出消息
    if (m_hookThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_hookThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    if (m_captureAndRenderThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_captureAndRenderThread.get()->native_handle()), WM_QUIT, 0, 0);
    }
    if (m_windowManagerThread.getId() != std::thread::id()) {
        PostThreadMessage(GetThreadId(m_windowManagerThread.get()->native_handle()), WM_QUIT, 0, 0);
    }

    // 等待线程结束
    if (m_captureAndRenderThread.get() && m_captureAndRenderThread.get()->joinable()) {
        m_captureAndRenderThread.get()->join();
    }
    LOG_DEBUG("Capture and render thread joined");
    if (m_hookThread.get() && m_hookThread.get()->joinable()) {
        m_hookThread.get()->join();
    }
    LOG_DEBUG("Hook thread joined");
    if (m_windowManagerThread.get() && m_windowManagerThread.get()->joinable()) {
        m_windowManagerThread.get()->join();
    }
    LOG_DEBUG("Window manager thread joined");
    if (m_captureSession) {
        m_captureSession.Close();
        m_captureSession = nullptr;
    }
    if (m_framePool) {
        m_framePool.Close();
        m_framePool = nullptr;
    }
    m_captureItem = nullptr;  

    if (m_frameLatencyWaitableObject) {
        CloseHandle(m_frameLatencyWaitableObject);
        m_frameLatencyWaitableObject = nullptr;
    }

    // 隐藏叠加层，恢复游戏窗口
    if (hideWindow) {
        RestoreGameWindow();
    }

    // 设置定时器调用 Cleanup
    if (!m_cleanupTimer.IsRunning()) {
        LOG_INFO("Starting cleanup timer");
        m_cleanupTimer.SetTimer(CLEANUP_TIMEOUT, [this]() {
            LOG_INFO("Cleanup timer fired");
            Cleanup();
        });
    }
}

void OverlayWindow::RestoreGameWindow() {
    ShowWindow(m_hwnd, SW_HIDE);
    LONG_PTR currentExStyle = GetWindowLongPtr(instance->m_gameWindow, GWL_EXSTYLE);
    currentExStyle &= ~WS_EX_LAYERED;
    SetWindowLongPtr(instance->m_gameWindow, GWL_EXSTYLE, currentExStyle);
    int left = (instance->m_screenWidth - instance->m_cachedGameWidth) / 2;
    int top = (instance->m_screenHeight - instance->m_cachedGameHeight) / 2;
    SetWindowPos(instance->m_gameWindow, HWND_TOP, left, top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOZORDER);
}


void OverlayWindow::Cleanup() {
    {
        std::lock_guard<std::mutex> lock(m_captureStateMutex);
        // 释放 Direct3D 资源
        m_renderTarget.Reset();
        m_swapChain.Reset();
        m_vertexBuffer.Reset();
        m_inputLayout.Reset();
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_sampler.Reset();
        m_blendState.Reset();
        m_shaderResourceView.Reset();

        // 清理 D3D 设备资源
        if (m_context) {
            // 重要：在释放设备前，先清除设备上下文中所有绑定的资源引用
            m_context->ClearState();
            m_context->Flush();
            m_context.Reset();
        }
        if (m_swapChain) {
            m_swapChain.Reset();
        }
        
        m_device.Reset();
        m_winrtDevice = nullptr;
    }

    // 标记 D3D 初始化状态
    m_d3dInitialized = false;
}

bool OverlayWindow::InitializeD3D() {
    LOG_DEBUG("Initializing Direct3D");

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
        LOG_ERROR("Failed to create D3D11 device, HRESULT: 0x%08X", hr);
        return false;
    }

    // 获取DXGI设备
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get DXGI device, HRESULT: 0x%08X", hr);
        return false;
    }

    // 设置最大帧延迟为3
    hr = dxgiDevice->SetMaximumFrameLatency(3);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set maximum frame latency, HRESULT: 0x%08X", hr);
        return false;
    }

    // 获取DXGI Factory6
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory6));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DXGI Factory6, HRESULT: 0x%08X", hr);
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
        LOG_ERROR("Failed to get high performance adapter, HRESULT: 0x%08X", hr);
        return false;
    }

    // 获取适配器信息
    DXGI_ADAPTER_DESC3 desc = {};
    hr = adapter4->GetDesc3(&desc);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Using graphics adapter: %ls", desc.Description);
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
            LOG_INFO("Variable refresh rate (VRR) is supported");
        } else {
            LOG_INFO("Variable refresh rate (VRR) is not supported");
        }
    } else {
        LOG_INFO("Failed to check VRR support");
    }
    
    // 添加帧延迟等待对象标志
    tearingFlags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    // 创建交换链描述
    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = m_windowWidth > 0 ? m_windowWidth : m_screenWidth;
    scd.Height = m_windowHeight > 0 ? m_windowHeight : m_screenHeight;
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
        LOG_ERROR("Failed to create swap chain, HRESULT: 0x%08X", hr);
        return false;
    }

    // 获取 IDXGISwapChain4 接口
    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IDXGISwapChain4 interface, HRESULT: 0x%08X", hr);
        return false;
    }

    // 获取帧延迟等待对象
    m_frameLatencyWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
    if (m_frameLatencyWaitableObject) {
        LOG_INFO("Frame latency waitable object is supported");
    } else {
        LOG_INFO("Frame latency waitable object is not supported");
    }

    // 禁用Alt+Enter全屏切换
    factory6->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (!CreateRenderTarget()) {
        LOG_ERROR("Failed to create render target");
        return false;
    }

    if (!CreateShaderResources()) {
        LOG_ERROR("Failed to create shader resources");
        return false;
    }

    LOG_DEBUG("Direct3D initialization completed successfully");
    return true;
}

bool OverlayWindow::ResizeSwapChain() {
    if (!m_swapChain) {
        LOG_ERROR("Swap chain not initialized");
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
        LOG_ERROR("Failed to get swap chain description, HRESULT: 0x%08X", hr);
        return false;
    }

    // 调整交换链缓冲区大小
    hr = m_swapChain->ResizeBuffers(
        4,
        m_windowWidth > 0 ? m_windowWidth : m_screenWidth,
        m_windowHeight > 0 ? m_windowHeight : m_screenHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        desc.Flags    // 保持原有标志
    );

    if (FAILED(hr)) {
        LOG_ERROR("Failed to resize swap chain buffers, HRESULT: 0x%08X", hr);
        return false;
    }

    // 重新创建渲染目标
    if (!CreateRenderTarget()) {
        LOG_ERROR("Failed to recreate render target after resize");
        return false;
    }

    LOG_DEBUG("Swap chain resized successfully");
    return true;
}

bool OverlayWindow::CreateRenderTarget() {
    // 获取后缓冲
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get back buffer, HRESULT: 0x%08X", hr);
        return false;
    }

    // 创建渲染目标视图
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create render target view, HRESULT: 0x%08X", hr);
        return false;
    }

    return true;
}

bool OverlayWindow::CreateShaderResources() {
    // 编译和创建着色器
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        std::string errorMsg = "Failed to compile vertex shader";
        if (errorBlob) {
            errorMsg += ": ";
            errorMsg += static_cast<char*>(errorBlob->GetBufferPointer());
        }
        LOG_ERROR("%s", errorMsg.c_str());
        return false;
    }

    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        std::string errorMsg = "Failed to compile pixel shader";
        if (errorBlob) {
            errorMsg += ": ";
            errorMsg += static_cast<char*>(errorBlob->GetBufferPointer());
        }
        LOG_ERROR("%s", errorMsg.c_str());
        return false;
    }

    // 创建着色器
    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex shader, HRESULT: 0x%08X", hr);
        return false;
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create pixel shader, HRESULT: 0x%08X", hr);
        return false;
    }

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    
    hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create input layout, HRESULT: 0x%08X", hr);
        return false;
    }
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
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex buffer, HRESULT: 0x%08X", hr);
        return false;
    }

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

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create sampler state, HRESULT: 0x%08X", hr);
        return false;
    }

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
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create blend state, HRESULT: 0x%08X", hr);
        return false;
    }

    return true;
}

void OverlayWindow::OnFrameArrived() {
    if (!m_running) return;
    if (!m_framePool) return;
    
    if (auto frame = m_framePool.TryGetNextFrame()) {
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        if (!frameTexture) return;

        // 直接为捕获的纹理创建 SRV
        m_shaderResourceView.Reset(); // 释放之前的 SRV

        D3D11_TEXTURE2D_DESC desc;
        frameTexture->GetDesc(&desc);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        {
            std::lock_guard<std::mutex> lock(m_captureStateMutex);
            HRESULT hr = m_device->CreateShaderResourceView(
                frameTexture.Get(), &srvDesc, &m_shaderResourceView);
            // 设置着色器资源视图
            if (SUCCEEDED(hr)) {
                m_context->PSSetShaderResources(0, 1, m_shaderResourceView.GetAddressOf());
            }
            PerformRendering();
        }   
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
    viewport.Width = static_cast<float>(m_windowWidth > 0 ? m_windowWidth : m_screenWidth);
    viewport.Height = static_cast<float>(m_windowHeight > 0 ? m_windowHeight : m_screenHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // 设置混合状态（只需要设置一次）
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
            case WM_DESTROY:
                instance->Cleanup();
                return 0;

            case WM_SHOW_OVERLAY: {
                ShowWindow(hwnd, SW_SHOWNA);
                
                // 计算居中位置
                int screenWidth = instance->m_screenWidth;
                int screenHeight = instance->m_screenHeight;
                int left = (screenWidth - instance->m_windowWidth) / 2;
                int top = (screenHeight - instance->m_windowHeight) / 2;
                
                // 添加分层窗口样式
                LONG exStyle = GetWindowLong(instance->m_gameWindow, GWL_EXSTYLE);
                SetWindowLong(instance->m_gameWindow, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                
                // 设置透明度 (透明度1，但仍可点击)
                SetLayeredWindowAttributes(instance->m_gameWindow, 0, 1, LWA_ALPHA);

                // 设置窗口位置和大小
                SetWindowPos(hwnd, nullptr,
                    left, top,
                    instance->m_windowWidth, instance->m_windowHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                    
                SetWindowPos(instance->m_gameWindow, hwnd, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE);
                    
                UpdateWindow(hwnd);
                return 0;
            }
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
