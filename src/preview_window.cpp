#include "preview_window.hpp"
#include <windowsx.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <d3dcompiler.h>
#include <stdexcept>

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

template<typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    Microsoft::WRL::ComPtr<T> result;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
    return result;
}

PreviewWindow* PreviewWindow::instance = nullptr;

PreviewWindow::PreviewWindow() : hwnd(nullptr), isDragging(false) {
    instance = this;
    winrt::init_apartment();
}

PreviewWindow::~PreviewWindow() {
    StopCapture();
    Cleanup();
    instance = nullptr;
}

bool PreviewWindow::StartCapture(HWND targetWindow) {
    if (!targetWindow) return false;

    // 首次启动预览时初始化 D3D
    if (!device && !InitializeD3D()) {
        return false;
    }

    // 获取游戏窗口的实际尺寸（包括溢出屏幕的部分）
    RECT windowRect;
    GetWindowRect(targetWindow, &windowRect);
    
    // 获取窗口客户区的实际尺寸
    RECT clientRect;
    GetClientRect(targetWindow, &clientRect);
    
    // 计算实际的窗口尺寸
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    m_aspectRatio = static_cast<float>(height) / width;

    // 计算预览窗口的实际尺寸
    int actualWidth, actualHeight;
    if (width >= height) {
        // 宽屏，使用理想尺寸作为宽度上限
        actualWidth = m_idealSize;
        actualHeight = static_cast<int>(m_idealSize * m_aspectRatio);
    } else {
        // 窄屏，使用理想尺寸作为高度上限
        actualHeight = m_idealSize;
        actualWidth = static_cast<int>(m_idealSize / m_aspectRatio);
    }

    // 确保不小于最小尺寸
    actualWidth = max(actualWidth, MIN_WIDTH);
    actualHeight = max(actualHeight, MIN_HEIGHT);

    // 如果是首次显示，设置默认位置（左上角）
    if (m_isFirstShow) {
        m_isFirstShow = false;  // 标记为非首次显示
        int x = 20;  // 距离左边缘20像素
        int y = 20;  // 距离上边缘20像素
        SetWindowPos(hwnd, nullptr, x, y, actualWidth, actualHeight + TITLE_HEIGHT, 
                    SWP_NOZORDER | SWP_SHOWWINDOW);
    } else {
        // 如果窗口已经显示，只更新尺寸保持位置不变
        RECT previewRect;
        GetWindowRect(hwnd, &previewRect);
        SetWindowPos(hwnd, nullptr, 0, 0, actualWidth, actualHeight + TITLE_HEIGHT, 
                    SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    // 停止现有的捕获
    StopCapture();

    // 创建 WinRT D3D 设备
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    winrt::com_ptr<::IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
    if (FAILED(hr)) return false;

    winrtDevice = inspectable.as<IDirect3DDevice>();
    if (!winrtDevice) return false;

    // 创建捕获项
    auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    hr = interop->CreateForWindow(
        targetWindow,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(captureItem))
    );
    if (FAILED(hr)) return false;

    // 创建帧池，使用实际的窗口尺寸
    framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
        winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { width, height }  // 使用实际的窗口尺寸
    );

    // 设置帧到达回调
    framePool.FrameArrived([this](auto&& sender, auto&&) {
        OnFrameArrived();
    });

    // 开始捕获
    captureSession = framePool.CreateCaptureSession(captureItem);
    captureSession.IsCursorCaptureEnabled(false);  // 禁用鼠标捕获
    captureSession.IsBorderRequired(false);        // 禁用边框
    captureSession.StartCapture();

    // 显示窗口
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return true;
}

void PreviewWindow::StopCapture() {
    if (captureSession) {
        captureSession.Close();
        captureSession = nullptr;
    }
    if (framePool) {
        framePool.Close();
        framePool = nullptr;
    }
    captureItem = nullptr;

    // 隐藏窗口
    ShowWindow(hwnd, SW_HIDE);
}

void PreviewWindow::OnFrameArrived() {
    // 获取互斥锁
    std::lock_guard<std::mutex> lock(renderTargetMutex);
    
    // 如果渲染目标无效，跳过这一帧
    if (!renderTarget) {
        return;
    }

    if (auto frame = framePool.TryGetNextFrame()) {
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
            if (SUCCEEDED(device->CreateShaderResourceView(frameTexture.Get(), &srvDesc, &newSRV))) {
                shaderResourceView = newSRV;
            }
        }

        // 渲染帧
        if (shaderResourceView && renderTarget) {  // 添加renderTarget检查
            // 清除背景
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            context->ClearRenderTargetView(renderTarget.Get(), clearColor);

            // 设置渲染目标
            ID3D11RenderTargetView* views[] = { renderTarget.Get() };
            context->OMSetRenderTargets(1, views, nullptr);

            // 设置视口
            D3D11_VIEWPORT viewport = {};
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            viewport.Width = static_cast<float>(clientRect.right - clientRect.left);
            viewport.Height = static_cast<float>(clientRect.bottom - clientRect.top);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            context->RSSetViewports(1, &viewport);

            // 设置着色器和资源
            context->IASetInputLayout(inputLayout.Get());
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            UINT stride = sizeof(PreviewWindow::Vertex);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
            context->VSSetShader(vertexShader.Get(), nullptr, 0);
            context->PSSetShader(pixelShader.Get(), nullptr, 0);
            context->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
            context->PSSetSamplers(0, 1, sampler.GetAddressOf());

            // 设置混合状态
            float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            context->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);

            // 绘制
            context->Draw(4, 0);

            // 显示，不等待垂直同步
            swapChain->Present(0, 0);
        }
    }
}

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

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr)) {
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    if (FAILED(hr)) {
        return false;
    }

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &inputLayout);
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

    hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
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

    if (FAILED(device->CreateSamplerState(&samplerDesc, &sampler))) {
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

    if (FAILED(device->CreateBlendState(&blendDesc, &blendState))) {
        return false;
    }

    return true;
}

bool PreviewWindow::Initialize(HINSTANCE hInstance) {
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
    hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        L"PreviewWindowClass",
        L"预览窗口",
        WS_POPUP,  // 移除 WS_VISIBLE
        0, 0,  // 初始位置不重要，会在 StartCapture 中设置
        m_idealSize, m_idealSize + TITLE_HEIGHT,  // 初始尺寸为理想尺寸
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!hwnd) {
        return false;
    }

    // 设置窗口透明度
    SetLayeredWindowAttributes(hwnd, 0, 204, LWA_ALPHA);

    return true;
}

void PreviewWindow::Cleanup() {
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

bool PreviewWindow::InitializeD3D() {
    // 创建交换链描述
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;  // 使用双缓冲
    scd.BufferDesc.Width = m_idealSize;
    scd.BufferDesc.Height = m_idealSize;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
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
        &scd, &swapChain, &device, &featureLevel, &context
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

    return true;
}

bool PreviewWindow::CreateRenderTarget() {
    // 获取后缓冲
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        return false;
    }

    // 创建渲染目标视图
    hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTarget);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

LRESULT CALLBACK PreviewWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (instance) {
        switch (message) {
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            
            // 获取窗口客户区大小
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // 定义边框感应区域宽度
            const int BORDER_WIDTH = 8;
            
            // 如果在标题栏区域
            if (pt.y < TITLE_HEIGHT) {
                return HTCAPTION;
            }
            
            // 检查边框区域
            if (pt.x <= BORDER_WIDTH) {
                if (pt.y <= BORDER_WIDTH) return HTTOPLEFT;
                if (pt.y >= rc.bottom - BORDER_WIDTH) return HTBOTTOMLEFT;
                return HTLEFT;
            }
            if (pt.x >= rc.right - BORDER_WIDTH) {
                if (pt.y <= BORDER_WIDTH) return HTTOPRIGHT;
                if (pt.y >= rc.bottom - BORDER_WIDTH) return HTBOTTOMRIGHT;
                return HTRIGHT;
            }
            if (pt.y <= BORDER_WIDTH) return HTTOP;
            if (pt.y >= rc.bottom - BORDER_WIDTH) return HTBOTTOM;
            
            return HTCLIENT;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口客户区大小
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // 绘制标题栏背景
            RECT titleRect = { 0, 0, rc.right, TITLE_HEIGHT };
            HBRUSH titleBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &titleRect, titleBrush);
            DeleteObject(titleBrush);

            // 绘制标题文本
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(51, 51, 51));
            HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            
            titleRect.left += 12; // 文本左边距
            DrawTextW(hdc, L"预览窗口", -1, &titleRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);

            // 绘制分隔线
            RECT sepRect = { 0, TITLE_HEIGHT - 1, rc.right, TITLE_HEIGHT };
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
            int height = rect->bottom - rect->top - TITLE_HEIGHT;  // 减去标题栏高度得到客户区高度
            
            // 根据拖动方向调整大小
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                    // 用户调整宽度，相应调整高度
                    width = max(width, MIN_WIDTH);
                    height = static_cast<int>(width * instance->m_aspectRatio);
                    height = max(height, MIN_HEIGHT);  // 确保高度不小于最小值
                    rect->bottom = rect->top + height + TITLE_HEIGHT;  // 加回标题栏高度
                    rect->right = rect->left + static_cast<int>(height / instance->m_aspectRatio);
                    break;

                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                    // 用户调整高度，相应调整宽度
                    height = max(height, MIN_HEIGHT);  // 确保客户区高度不小于最小值
                    width = static_cast<int>(height / instance->m_aspectRatio);
                    width = max(width, MIN_WIDTH);  // 确保宽度不小于最小值
                    
                    if (wParam == WMSZ_BOTTOM) {
                        rect->bottom = rect->top + height + TITLE_HEIGHT;  // 加回标题栏高度
                    } else {
                        rect->top = rect->bottom - (height + TITLE_HEIGHT);  // 从底部向上调整
                    }
                    
                    if (width > MIN_WIDTH) {
                        if (wParam == WMSZ_BOTTOM) {
                            rect->right = rect->left + width;
                        } else {
                            rect->left = rect->right - width;
                        }
                    }
                    break;

                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    // 对角调整时，以宽度为准
                    width = max(width, MIN_WIDTH);
                    height = static_cast<int>(width * instance->m_aspectRatio);
                    height = max(height, MIN_HEIGHT);  // 确保高度不小于最小值
                    
                    if (wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT) {
                        rect->bottom = rect->top + height + TITLE_HEIGHT;
                    } else {
                        rect->top = rect->bottom - (height + TITLE_HEIGHT);
                    }
                    
                    if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                        rect->left = rect->right - width;
                    } else {
                        rect->right = rect->left + width;
                    }
                    break;
            }
            return TRUE;
        }

        case WM_LBUTTONDOWN:
            instance->isDragging = true;
            instance->dragStart.x = LOWORD(lParam);
            instance->dragStart.y = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            if (instance->isDragging) {
                int xPos = LOWORD(lParam);
                int yPos = HIWORD(lParam);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                int deltaX = xPos - instance->dragStart.x;
                int deltaY = yPos - instance->dragStart.y;
                SetWindowPos(hwnd, nullptr,
                    rect.left + deltaX,
                    rect.top + deltaY,
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;

        case WM_LBUTTONUP:
            if (instance->isDragging) {
                instance->isDragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_SIZE: {
            if (!instance->device) return 0;
            
            {
                // 获取互斥锁
                std::lock_guard<std::mutex> lock(instance->renderTargetMutex);
                
                // 释放旧的渲染目标
                instance->renderTarget = nullptr;

                // 调整交换链大小
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                HRESULT hr = instance->swapChain->ResizeBuffers(
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
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
} 