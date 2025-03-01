#include "preview_window.hpp"
#include <windowsx.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <d3dcompiler.h>
#include <stdexcept>
#include "logger.hpp"

namespace {
    // 着色器代码
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

PreviewWindow* PreviewWindow::instance = nullptr;

PreviewWindow::PreviewWindow() : m_hwnd(nullptr), m_isDragging(false) {
    instance = this;
    winrt::init_apartment();

    // 获取系统 DPI
    HDC hdc = GetDC(NULL);
    m_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    // 计算理想尺寸范围
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    m_minIdealSize = min(screenWidth, screenHeight) / 10;
    m_maxIdealSize = max(screenWidth, screenHeight);
    m_idealSize = screenHeight / 2;  // 初始值设为屏幕高度的一半

    UpdateDpiDependentResources();
}

PreviewWindow::~PreviewWindow() {
    StopCapture();
    Cleanup();
    if (m_cleanupTimer.IsRunning()) {
        m_cleanupTimer.Cancel();
    }
    instance = nullptr;
}

bool PreviewWindow::StartCapture(HWND targetWindow, int customWidth, int customHeight) {
    if (m_cleanupTimer.IsRunning()) {
        LOG_DEBUG("Canceling cleanup timer due to new capture");
        m_cleanupTimer.Cancel();
    }

    if (!targetWindow) return false;

    // 保存游戏窗口句柄
    m_gameWindow = targetWindow;

    // 计算实际的窗口尺寸
    int width, height;
    
    if (customWidth > 0 && customHeight > 0) {
        // 如果提供了自定义尺寸，则使用它
        width = customWidth;
        height = customHeight;
    } else {
        // 否则，获取窗口客户区的实际尺寸
        RECT clientRect;
        GetClientRect(targetWindow, &clientRect);
        width = clientRect.right - clientRect.left;
        height = clientRect.bottom - clientRect.top;
    }
    
    m_aspectRatio = static_cast<float>(height) / width;

    // 根据宽高比计算实际窗口尺寸
    if (m_aspectRatio >= 1.0f) {
        // 高度大于等于宽度，使用理想尺寸作为高度
        m_windowHeight = m_idealSize;
        m_windowWidth = static_cast<int>(m_windowHeight / m_aspectRatio);
    } else {
        // 宽度大于高度，使用理想尺寸作为宽度
        m_windowWidth = m_idealSize;
        m_windowHeight = static_cast<int>(m_windowWidth * m_aspectRatio);
    }

    // 初始化 D3D 资源
    if (!m_d3dInitialized) {
        if (!InitializeD3D()) {
            return false;
        }
        m_d3dInitialized = true;
    }

    // 如果是首次显示，设置默认位置（左上角）
    if (m_isFirstShow) {
        m_isFirstShow = false;  // 标记为非首次显示
        int x = 20;  // 距离左边缘20像素
        int y = 20;  // 距离上边缘20像素
        SetWindowPos(m_hwnd, nullptr, x, y, m_windowWidth, m_windowHeight, 
                    SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    } else {
        // 如果窗口已经显示，只更新尺寸保持位置不变
        RECT previewRect;
        GetWindowRect(m_hwnd, &previewRect);
        SetWindowPos(m_hwnd, nullptr, 0, 0, m_windowWidth, m_windowHeight,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }

    // 创建 DirectX 设备
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    // 创建 WinRT 设备
    winrt::com_ptr<::IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
    if (FAILED(hr)) return false;

    m_winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    if (!m_winrtDevice) return false;

    // 创建捕获项
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    hr = interop->CreateForWindow(
        targetWindow,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(m_captureItem))
    );
    if (FAILED(hr)) return false;

    // 创建帧池，使用实际的窗口尺寸
    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { width, height }  // 使用实际的窗口尺寸
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
        m_captureSession.IsCursorCaptureEnabled(false);
    }
    
    // 尝试禁用边框 - 使用ApiInformation检查API是否可用
    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
        L"IsBorderRequired")) 
    {
        // Windows 10 2104 (Build 20348)
        m_captureSession.IsBorderRequired(false);
    }
    
    try {
        // 开始捕获
        m_captureSession.StartCapture();
    }
    catch (...) {
        LOG_ERROR("Unknown error occurred while starting capture session");
        return false;
    }

    // 显示窗口
    ShowWindow(m_hwnd, SW_SHOWNA);
    UpdateWindow(m_hwnd);

    return true;
}

void PreviewWindow::StopCapture() {
    if (m_captureSession) {
        m_captureSession.Close();
        m_captureSession = nullptr;
    }
    if (m_framePool) {
        m_framePool.FrameArrived(m_frameArrivedToken);
        m_framePool.Close();
        m_framePool = nullptr;
    }
    m_captureItem = nullptr;

    // 隐藏窗口
    ShowWindow(m_hwnd, SW_HIDE);

    // 设置定时器调用 Cleanup
    if (!m_cleanupTimer.IsRunning()) {
        LOG_INFO("Starting cleanup timer");
        m_cleanupTimer.SetTimer(CLEANUP_TIMEOUT, [this]() {
            LOG_INFO("Cleanup timer fired");
            Cleanup();
        });
    }
}

void PreviewWindow::OnFrameArrived() {
    // 获取互斥锁
    std::lock_guard<std::mutex> lock(m_renderTargetMutex);
    
    // 如果渲染目标无效，跳过这一帧
    if (!m_renderTarget) {
        return;
    }

    if (auto frame = m_framePool.TryGetNextFrame()) {
        // 获取帧的纹理
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        
        // 更新着色器资源视图
        if (frameTexture) {
            D3D11_TEXTURE2D_DESC desc;
            frameTexture->GetDesc(&desc);

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newSRV;
            if (SUCCEEDED(m_device->CreateShaderResourceView(frameTexture.Get(), &srvDesc, &newSRV))) {
                m_shaderResourceView = newSRV;
            }
        }

        // 渲染帧
        if (m_shaderResourceView && m_renderTarget) {  // 添加renderTarget检查
            // 清除背景
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);

            // 设置渲染目标
            ID3D11RenderTargetView* views[] = { m_renderTarget.Get() };
            m_context->OMSetRenderTargets(1, views, nullptr);

            // 设置视口
            D3D11_VIEWPORT viewport = {};
            RECT clientRect;
            GetClientRect(m_hwnd, &clientRect);
            viewport.Width = static_cast<float>(clientRect.right - clientRect.left);
            viewport.Height = static_cast<float>(clientRect.bottom - clientRect.top);
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;  // 从窗口顶部开始渲染
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            m_context->RSSetViewports(1, &viewport);

            // 设置着色器和资源
            m_context->IASetInputLayout(m_inputLayout.Get());
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            UINT stride = sizeof(PreviewWindow::Vertex);
            UINT offset = 0;
            m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
            m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
            m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
            m_context->PSSetShaderResources(0, 1, m_shaderResourceView.GetAddressOf());
            m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

            // 设置混合状态
            float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);

            // 绘制游戏画面
            m_context->Draw(4, 0);

            // 在渲染游戏画面之后，更新并渲染视口框
            UpdateViewportRect();
            RenderViewport();

            // 显示
            m_swapChain->Present(0, 0);
        }
    }
}

bool PreviewWindow::CreateShaderResources() {
    // 编译着色器
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        return false;
    }

    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) {
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
        return false;
    }

    // 创建顶点缓冲
    PreviewWindow::Vertex vertices[] = {
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

    if (FAILED(m_device->CreateSamplerState(&samplerDesc, &m_sampler))) {
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

    if (FAILED(m_device->CreateBlendState(&blendDesc, &m_blendState))) {
        return false;
    }

    return true;
}

bool PreviewWindow::Initialize(HINSTANCE hInstance, HWND mainHwnd) {
    // 设置主窗口句柄
    m_mainHwnd = mainHwnd;

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PreviewWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  // 添加背景色
    wc.style = CS_HREDRAW | CS_VREDRAW;            // 添加窗口样式
    
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // 计算理想尺寸（屏幕高度的50%）
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    m_idealSize = static_cast<int>(screenHeight * 0.5);

    // 创建窗口，但初始不显示
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        L"PreviewWindowClass",
        L"预览窗口",
        WS_POPUP,  // 移除 WS_VISIBLE
        0, 0,  // 初始位置不重要，会在 StartCapture 中设置
        m_idealSize, m_idealSize + TITLE_HEIGHT,  // 初始尺寸为理想尺寸
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!m_hwnd) {
        return false;
    }

    // 设置窗口透明度
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    // 设置窗口圆角和阴影
    MARGINS margins = {1, 1, 1, 1};  // 四边均匀阴影效果
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    
    BOOL value = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));
    
    // Windows 11 风格的圆角
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;  // 使用小圆角
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    return true;
}

void PreviewWindow::Cleanup() {
    {
        std::lock_guard<std::mutex> lock(m_renderTargetMutex);

        // 清理视口框资源
        m_viewportVertexBuffer.Reset();
        m_viewportVS.Reset();
        m_viewportPS.Reset();
        m_viewportInputLayout.Reset();

        // 清理主渲染资源
        m_shaderResourceView.Reset();
        m_vertexBuffer.Reset();
        m_inputLayout.Reset();
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_sampler.Reset();
        m_blendState.Reset();
        m_renderTarget.Reset();

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

bool PreviewWindow::InitializeD3D() {
    // 创建交换链描述
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;  // 使用双缓冲
    scd.BufferDesc.Width = m_windowWidth > 0 ? m_windowWidth : m_idealSize;
    scd.BufferDesc.Height = m_windowHeight > 0 ? m_windowHeight : m_idealSize;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = m_hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // 使用现代的交换链模式
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // 创建设备和交换链
    D3D_FEATURE_LEVEL featureLevel;
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION,
        &scd, &m_swapChain, &m_device, &featureLevel, &m_context
    );

    if (FAILED(hr)) {
        return false;
    }

    if (!CreateRenderTarget()) {
        return false;
    }

    if (!CreateShaderResources()) {
        return false;
    }

    // 创建视口框资源
    if (!CreateViewportResources()) {
        return false;
    }

    return true;
}

bool PreviewWindow::CreateRenderTarget() {
    // 获取后缓冲
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        return false;
    }

    // 创建渲染目标视图
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool PreviewWindow::CreateViewportResources() {
    // 着色器代码
    const char* viewportVSCode = R"(
        struct VS_INPUT {
            float2 pos : POSITION;
            float4 color : COLOR;
        };
        
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float4 color : COLOR;
        };
        
        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos.x * 2 - 1, -(input.pos.y * 2 - 1), 0, 1);
            output.color = input.color;
            return output;
        }
    )";

    const char* viewportPSCode = R"(
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float4 color : COLOR;
        };
        
        float4 main(PS_INPUT input) : SV_Target {
            return input.color;
        }
    )";

    // 编译着色器
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
#endif

    if (FAILED(D3DCompile(viewportVSCode, strlen(viewportVSCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", compileFlags, 0, &vsBlob, &errorBlob))) {
        return false;
    }

    if (FAILED(D3DCompile(viewportPSCode, strlen(viewportPSCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", compileFlags, 0, &psBlob, &errorBlob))) {
        return false;
    }

    // 创建着色器
    if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_viewportVS))) {
        return false;
    }

    if (FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_viewportPS))) {
        return false;
    }

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &m_viewportInputLayout))) {
        return false;
    }

    // 创建顶点缓冲区
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(ViewportVertex) * 5;  // 5个顶点绘制矩形框
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(m_device->CreateBuffer(&bufferDesc, nullptr, &m_viewportVertexBuffer))) {
        return false;
    }

    return true;
}

void PreviewWindow::UpdateViewportRect() {
    // 获取预览窗口客户区大小
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
    float previewHeight = static_cast<float>(clientRect.bottom - clientRect.top - TITLE_HEIGHT);

    // 获取游戏窗口当前位置和尺寸
    RECT gameRect;
    GetWindowRect(m_gameWindow, &gameRect);
    m_gameWindowRect = gameRect;  // 更新游戏窗口位置

    // 确保游戏窗口尺寸有效
    if (m_gameWindowRect.right <= m_gameWindowRect.left || m_gameWindowRect.bottom <= m_gameWindowRect.top) {
        return;
    }

    // 计算游戏窗口和屏幕的尺寸
    float gameWidth = static_cast<float>(m_gameWindowRect.right - m_gameWindowRect.left);
    float gameHeight = static_cast<float>(m_gameWindowRect.bottom - m_gameWindowRect.top);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 判断游戏窗口是否完全可见
    m_isGameWindowFullyVisible = (gameWidth <= screenWidth && gameHeight <= screenHeight &&
                                m_gameWindowRect.left >= 0 && m_gameWindowRect.top >= 0 &&
                                m_gameWindowRect.right <= screenWidth && m_gameWindowRect.bottom <= screenHeight);
    
    // 更新视口可见性
    m_viewportVisible = !m_isGameWindowFullyVisible;

    // 如果游戏窗口完全可见，不需要更新视口矩形
    if (m_isGameWindowFullyVisible) {
        return;
    }

    // 计算缩放比例
    float scaleX = previewWidth / gameWidth;
    float scaleY = previewHeight / gameHeight;

    // 计算游戏窗口相对于屏幕的位置（考虑负值）
    float gameLeft = static_cast<float>(m_gameWindowRect.left);
    float gameTop = static_cast<float>(m_gameWindowRect.top);

    // 计算视口在预览窗口中的位置
    float viewportLeft = (-gameLeft / gameWidth) * previewWidth;
    float viewportTop = (-gameTop / gameHeight) * previewHeight;

    // 设置视口矩形位置
    m_viewportRect.left = static_cast<LONG>(viewportLeft);
    m_viewportRect.top = static_cast<LONG>(viewportTop) + TITLE_HEIGHT;
    m_viewportRect.right = m_viewportRect.left + static_cast<LONG>(screenWidth * scaleX);
    m_viewportRect.bottom = m_viewportRect.top + static_cast<LONG>(screenHeight * scaleY);

    // 更新顶点缓冲区
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(m_context->Map(m_viewportVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        ViewportVertex* vertices = static_cast<ViewportVertex*>(mappedResource.pData);
        
        // 转换到归一化坐标 (0-1)
        float left = static_cast<float>(m_viewportRect.left) / previewWidth;
        float top = static_cast<float>(m_viewportRect.top - TITLE_HEIGHT) / previewHeight;
        float right = static_cast<float>(m_viewportRect.right) / previewWidth;
        float bottom = static_cast<float>(m_viewportRect.bottom - TITLE_HEIGHT) / previewHeight;
        
        // 设置颜色 RGB(255, 160, 80)
        DirectX::XMFLOAT4 color = {255.0f/255.0f, 160.0f/255.0f, 80.0f/255.0f, 1.0f};
        
        // 设置顶点数据（线段条带）
        vertices[0] = {{left,  top},    color};     // 左上
        vertices[1] = {{right, top},    color};     // 右上
        vertices[2] = {{right, bottom}, color};     // 右下
        vertices[3] = {{left,  bottom}, color};     // 左下
        vertices[4] = {{left,  top},    color};     // 回到左上
        
        m_context->Unmap(m_viewportVertexBuffer.Get(), 0);
    }
}

void PreviewWindow::RenderViewport() {
    if (!m_viewportVisible || !m_viewportVertexBuffer) {
        return;
    }

    // 设置渲染状态
    UINT stride = sizeof(ViewportVertex);
    UINT offset = 0;
    m_context->IASetInputLayout(m_viewportInputLayout.Get());
    m_context->IASetVertexBuffers(0, 1, m_viewportVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    m_context->VSSetShader(m_viewportVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_viewportPS.Get(), nullptr, 0);

    // 创建并设置光栅化状态
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = FALSE;
    rasterizerDesc.MultisampleEnable = TRUE;
    rasterizerDesc.AntialiasedLineEnable = TRUE;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    m_device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    m_context->RSSetState(rasterizerState.Get());

    // 获取当前视口设置
    D3D11_VIEWPORT viewport;
    UINT numViewports = 1;
    m_context->RSGetViewports(&numViewports, &viewport);

    // 多次绘制，每次略微偏移视口位置
    const float offset_pixels = 0.5f;  // 偏移量（像素）
    const int num_draws = 5;           // 绘制次数

    for (int i = 0; i < num_draws; ++i) {
        // 计算当前偏移
        float offset_x = (i % 2) * offset_pixels;
        float offset_y = ((i + 1) % 2) * offset_pixels;

        // 设置偏移后的视口
        D3D11_VIEWPORT offsetViewport = viewport;
        offsetViewport.TopLeftX += offset_x;
        offsetViewport.TopLeftY += offset_y;
        m_context->RSSetViewports(1, &offsetViewport);

        // 绘制
        m_context->Draw(5, 0);
    }

    // 恢复原始视口
    m_context->RSSetViewports(1, &viewport);
}

LRESULT CALLBACK PreviewWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
        case WM_DPICHANGED: {
            // 处理 DPI 变化
            instance->m_dpi = HIWORD(wParam);
            instance->UpdateDpiDependentResources();

            // 使用系统建议的新窗口位置
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hwnd, nullptr,
                prcNewWindow->left, prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口客户区大小
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // 绘制标题栏背景
            RECT titleRect = { 0, 0, rc.right, instance->TITLE_HEIGHT };  // 使用DPI感知的高度
            HBRUSH titleBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &titleRect, titleBrush);
            DeleteObject(titleBrush);

            // 绘制标题文本
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(51, 51, 51));
            HFONT hFont = CreateFont(-instance->FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,  // 使用DPI感知的字体大小
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            
            titleRect.left += instance->FONT_SIZE;  // 使用字体大小作为文本左边距
            DrawTextW(hdc, L"预览窗口", -1, &titleRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);

            // 绘制分隔线
            RECT sepRect = { 0, instance->TITLE_HEIGHT - 1, rc.right, instance->TITLE_HEIGHT };  // 使用DPI感知的高度
            HBRUSH sepBrush = CreateSolidBrush(RGB(229, 229, 229));
            FillRect(hdc, &sepRect, sepBrush);
            DeleteObject(sepBrush);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            // 不再调用 PostQuitMessage，而是清理自身资源
            instance->Cleanup();
            return 0;

        case WM_SIZING: {
            RECT* rect = (RECT*)lParam;
            int width = rect->right - rect->left;
            int height = rect->bottom - rect->top;  // 不再减去标题栏高度
            
            // 根据拖动方向调整大小
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                    // 用户调整宽度，相应调整高度
                    width = max(width, instance->m_minIdealSize);  // 应用最小尺寸限制
                    height = static_cast<int>(width * instance->m_aspectRatio);
                    if (wParam == WMSZ_LEFT) {
                        rect->left = rect->right - width;
                    } else {
                        rect->right = rect->left + width;
                    }
                    rect->bottom = rect->top + height;
                    break;

                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                    // 用户调整高度，相应调整宽度
                    height = max(height, instance->m_minIdealSize);  // 应用最小尺寸限制
                    width = static_cast<int>(height / instance->m_aspectRatio);
                    if (wParam == WMSZ_TOP) {
                        rect->top = rect->bottom - height;
                    } else {
                        rect->bottom = rect->top + height;
                    }
                    if (wParam == WMSZ_TOP) {
                        rect->left = rect->right - width;
                    } else {
                        rect->right = rect->left + width;
                    }
                    break;

                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    // 对角调整时，以宽度为准
                    width = max(width, instance->m_minIdealSize);  // 应用最小尺寸限制
                    height = static_cast<int>(width * instance->m_aspectRatio);
                    
                    if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                        rect->left = rect->right - width;
                    } else {
                        rect->right = rect->left + width;
                    }
                    
                    if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
                        rect->top = rect->bottom - height;
                    } else {
                        rect->bottom = rect->top + height;
                    }
                    break;
            }

            // 更新理想尺寸（取新窗口宽高的较大值）
            instance->m_idealSize = max(width, height);

            return TRUE;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            
            // 如果点击在标题栏，保持原有的拖拽行为
            if (pt.y < instance->TITLE_HEIGHT) {
                instance->m_isDragging = true;
                instance->m_dragStart = pt;
                SetCapture(hwnd);
                return 0;
            }

            // 如果游戏窗口完全可见，整个预览窗口都可以用来拖拽
            if (instance->m_isGameWindowFullyVisible) {
                instance->m_isDragging = true;
                instance->m_dragStart = pt;
                SetCapture(hwnd);
                return 0;
            }

            // 获取预览窗口客户区大小
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
            float previewHeight = static_cast<float>(clientRect.bottom - clientRect.top - instance->TITLE_HEIGHT);

            // 检查是否点击在视口矩形上
            bool isOnViewport = (pt.x >= instance->m_viewportRect.left && pt.x <= instance->m_viewportRect.right &&
                               pt.y >= instance->m_viewportRect.top && pt.y <= instance->m_viewportRect.bottom);

            // 计算点击位置相对于预览区域的比例（0.0 - 1.0）
            float relativeX = static_cast<float>(pt.x) / previewWidth;
            float relativeY = static_cast<float>(pt.y - instance->TITLE_HEIGHT) / previewHeight;

            // 获取屏幕尺寸
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // 计算游戏窗口尺寸
            float gameWidth = static_cast<float>(instance->m_gameWindowRect.right - instance->m_gameWindowRect.left);
            float gameHeight = static_cast<float>(instance->m_gameWindowRect.bottom - instance->m_gameWindowRect.top);

            if (!isOnViewport) {
                // 如果点击在视口外，先移动游戏窗口使得点击位置成为屏幕中心
                // 计算新的游戏窗口位置
                float targetX, targetY;
                
                // 水平方向
                if (gameWidth <= screenWidth) {
                    targetX = (screenWidth - gameWidth) / 2;  // 居中
                } else {
                    targetX = -(relativeX * gameWidth);       // 拖动位置
                    targetX = max(targetX, -gameWidth + screenWidth);
                    targetX = min(targetX, 0.0f);
                }

                // 垂直方向
                if (gameHeight <= screenHeight) {
                    targetY = (screenHeight - gameHeight) / 2; // 居中
                } else {
                    targetY = -(relativeY * gameHeight);      // 拖动位置
                    targetY = max(targetY, -gameHeight + screenHeight);
                    targetY = min(targetY, 0.0f);
                }

                // 移动游戏窗口
                SetWindowPos(instance->m_gameWindow, nullptr,
                    static_cast<int>(targetX),
                    static_cast<int>(targetY),
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER);

                // 等待一帧以确保视口矩形位置更新
                instance->UpdateViewportRect();
            }

            // 开始拖拽视口（无论是点击在视口上还是视口外）
            instance->m_viewportDragging = true;
            instance->m_viewportDragStart = pt;
            // 如果点击在视口外，计算新的拖拽偏移（相对于视口中心）
            if (!isOnViewport) {
                instance->m_viewportDragOffset.x = static_cast<LONG>(screenWidth * previewWidth / (2 * gameWidth));
                instance->m_viewportDragOffset.y = static_cast<LONG>(screenHeight * previewHeight / (2 * gameHeight));
            } else {
                instance->m_viewportDragOffset.x = pt.x - instance->m_viewportRect.left;
                instance->m_viewportDragOffset.y = pt.y - instance->m_viewportRect.top;
            }
            SetCapture(hwnd);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (instance->m_isDragging) {
                int xPos = LOWORD(lParam);
                int yPos = HIWORD(lParam);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                int deltaX = xPos - instance->m_dragStart.x;
                int deltaY = yPos - instance->m_dragStart.y;
                SetWindowPos(hwnd, nullptr,
                    rect.left + deltaX,
                    rect.top + deltaY,
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER);
            }
            else if (instance->m_viewportDragging) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                
                // 获取预览窗口客户区大小
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
                float previewHeight = static_cast<float>(clientRect.bottom - clientRect.top - instance->TITLE_HEIGHT);

                // 计算新的相对位置
                float relativeX = static_cast<float>(pt.x - instance->m_viewportDragOffset.x) / previewWidth;
                float relativeY = static_cast<float>(pt.y - instance->m_viewportDragOffset.y - instance->TITLE_HEIGHT) / previewHeight;

                // 获取屏幕和游戏窗口尺寸
                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                float gameWidth = static_cast<float>(instance->m_gameWindowRect.right - instance->m_gameWindowRect.left);
                float gameHeight = static_cast<float>(instance->m_gameWindowRect.bottom - instance->m_gameWindowRect.top);

                // 计算新的游戏窗口位置
                float targetX, targetY;
                
                // 水平方向
                if (gameWidth <= screenWidth) {
                    targetX = (screenWidth - gameWidth) / 2;  // 居中
                } else {
                    targetX = -(relativeX * gameWidth);       // 拖动位置
                    targetX = max(targetX, -gameWidth + screenWidth);
                    targetX = min(targetX, 0.0f);
                }

                // 垂直方向
                if (gameHeight <= screenHeight) {
                    targetY = (screenHeight - gameHeight) / 2; // 居中
                } else {
                    targetY = -(relativeY * gameHeight);      // 拖动位置
                    targetY = max(targetY, -gameHeight + screenHeight);
                    targetY = min(targetY, 0.0f);
                }

                // 移动游戏窗口
                SetWindowPos(instance->m_gameWindow, nullptr,
                    static_cast<int>(targetX),
                    static_cast<int>(targetY),
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;

        case WM_LBUTTONUP:
            if (instance->m_isDragging) {
                instance->m_isDragging = false;
                ReleaseCapture();
            }
            else if (instance->m_viewportDragging) {
                instance->m_viewportDragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_SIZE: {
            if (!instance->m_device) return 0;
            
            {
                // 获取互斥锁
                std::lock_guard<std::mutex> lock(instance->m_renderTargetMutex);
                
                // 释放旧的渲染目标
                instance->m_renderTarget.Reset();

                // 调整交换链大小
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int width = clientRect.right - clientRect.left;
                int height = clientRect.bottom - clientRect.top;  // 不再减去标题栏高度

                // 更新理想尺寸（取当前窗口宽高的较大值）
                instance->m_idealSize = max(width, height);

                // 调整交换链大小
                HRESULT hr = instance->m_swapChain->ResizeBuffers(
                    0,  // 保持当前缓冲区数量
                    clientRect.right - clientRect.left,
                    clientRect.bottom - clientRect.top,
                    DXGI_FORMAT_UNKNOWN,  // 保持当前格式
                    0
                );

                if (SUCCEEDED(hr)) {
                    // 重新创建渲染目标
                    instance->CreateRenderTarget();
                }
            }
            
            return 0;
        }

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            
            // 获取窗口客户区大小
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // 如果在标题栏区域
            if (pt.y < instance->TITLE_HEIGHT) {
                return HTCAPTION;
            }
            
            // 检查边框区域
            if (pt.x <= instance->BORDER_WIDTH) {
                if (pt.y <= instance->BORDER_WIDTH) return HTTOPLEFT;
                if (pt.y >= rc.bottom - instance->BORDER_WIDTH) return HTBOTTOMLEFT;
                return HTLEFT;
            }
            if (pt.x >= rc.right - instance->BORDER_WIDTH) {
                if (pt.y <= instance->BORDER_WIDTH) return HTTOPRIGHT;
                if (pt.y >= rc.bottom - instance->BORDER_WIDTH) return HTBOTTOMRIGHT;
                return HTRIGHT;
            }
            if (pt.y <= instance->BORDER_WIDTH) return HTTOP;
            if (pt.y >= rc.bottom - instance->BORDER_WIDTH) return HTBOTTOM;
            
            return HTCLIENT;
        }

        case WM_MOUSEWHEEL: {
            // 如果鼠标在标题栏，不处理缩放
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y < instance->TITLE_HEIGHT) {
                return 0;
            }

            // 获取滚轮增量
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            
            // 计算新的理想尺寸（每次改变10%）
            int oldIdealSize = instance->m_idealSize;
            int newIdealSize = static_cast<int>(oldIdealSize * (1.0f + (delta > 0 ? 0.1f : -0.1f)));
            
            // 限制在最小最大理想尺寸范围内
            newIdealSize = std::clamp(newIdealSize, instance->m_minIdealSize, instance->m_maxIdealSize);
            
            if (newIdealSize != oldIdealSize) {
                // 保存新的理想尺寸
                instance->m_idealSize = newIdealSize;
                
                // 根据宽高比计算实际窗口尺寸
                int newWidth, newHeight;
                if (instance->m_aspectRatio >= 1.0f) {
                    // 高度大于等于宽度，使用理想尺寸作为高度
                    newHeight = newIdealSize;
                    newWidth = static_cast<int>(newHeight / instance->m_aspectRatio);
                } else {
                    // 宽度大于高度，使用理想尺寸作为宽度
                    newWidth = newIdealSize;
                    newHeight = static_cast<int>(newWidth * instance->m_aspectRatio);
                }
                
                // 获取当前窗口位置
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                
                // 计算鼠标相对于窗口的位置（0-1范围）
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                float relativeX = static_cast<float>(pt.x) / (clientRect.right - clientRect.left);
                float relativeY = static_cast<float>(pt.y - instance->TITLE_HEIGHT) / 
                                (clientRect.bottom - clientRect.top - instance->TITLE_HEIGHT);
                
                // 计算新的窗口位置（保持鼠标指向的点不变）
                int deltaWidth = newWidth - (windowRect.right - windowRect.left);
                int deltaHeight = newHeight - (windowRect.bottom - windowRect.top);
                int newX = windowRect.left - static_cast<int>(deltaWidth * relativeX);
                int newY = windowRect.top - static_cast<int>(deltaHeight * relativeY);
                
                // 更新窗口位置和大小
                SetWindowPos(hwnd, nullptr,
                    newX, newY,
                    newWidth, newHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            if (instance) {
                PostMessage(instance->m_mainHwnd, Constants::WM_PREVIEW_RCLICK, 0, 0);
            }
            return 0;
        }
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
} 

void PreviewWindow::UpdateDpiDependentResources() {
    // 计算DPI缩放因子
    float dpiScale = static_cast<float>(m_dpi) / 96.0f;

    // 更新所有DPI相关的尺寸
    TITLE_HEIGHT = static_cast<int>(BASE_TITLE_HEIGHT * dpiScale);
    FONT_SIZE = static_cast<int>(BASE_FONT_SIZE * dpiScale);
    BORDER_WIDTH = static_cast<int>(BASE_BORDER_WIDTH * dpiScale);

    // 如果窗口已创建，更新窗口
    if (m_hwnd) {
        // 获取当前窗口位置和大小
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        // 更新窗口大小
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // 强制重绘
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
} 