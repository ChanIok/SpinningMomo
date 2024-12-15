#include "window_capture.h"
#include <dxgi1_2.h>
#include <d3d11.h>
#include <vector>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

// Helper function to create IDirect3DDevice
winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WindowCapture::CreateD3DDevice(ID3D11Device* device)
{
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));
    
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice;
    winrt::check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), reinterpret_cast<::IInspectable**>(winrt::put_abi(winrtDevice))));
    
    return winrtDevice;
}

// Helper template for getting DXGI interface
template<typename T>
winrt::com_ptr<T> GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.try_as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
    return result;
}

WindowCapture::WindowCapture()
    : targetWindow(nullptr)
    , d3dDevice(nullptr)
    , d3dContext(nullptr)
    , stagingTexture(nullptr)
    , initialized(false)
{
    // 初始化WinRT
    winrt::init_apartment(winrt::apartment_type::single_threaded);
}

WindowCapture::~WindowCapture()
{
    Cleanup();
}

bool WindowCapture::Initialize()
{
    if (initialized) return true;
    
    if (!InitializeD3D()) {
        return false;
    }

    if (!InitializeGraphicsCapture()) {
        return false;
    }

    initialized = true;
    return true;
}

bool WindowCapture::InitializeD3D()
{
    // 创建D3D设备和上下文
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    #ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // 默认适配器
        D3D_DRIVER_TYPE_HARDWARE,   // 硬件渲染
        nullptr,                    // 不使用软件渲染
        createDeviceFlags,          // 创建标志
        featureLevels,             // 特性等级
        ARRAYSIZE(featureLevels),  // 特性等级数量
        D3D11_SDK_VERSION,         // SDK版本
        &d3dDevice,                // 返回的D3D设备
        &featureLevel,             // 返回的特性等级
        &d3dContext                // 返回的设备上下文
    );

    if (FAILED(hr)) {
        // 如果硬件渲染失败，尝试WARP渲染
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &d3dDevice,
            &featureLevel,
            &d3dContext
        );

        if (FAILED(hr)) {
            return false;
        }
    }

    return true;
}

bool WindowCapture::InitializeGraphicsCapture()
{
    try {
        // 创建WinRT D3D设备
        winrtDevice = CreateD3DDevice(d3dDevice);
        return true;
    }
    catch (winrt::hresult_error const& ex) {
        OutputDebugStringW(ex.message().c_str());
        return false;
    }
}

bool WindowCapture::CaptureWindow(const std::wstring& windowTitle)
{
    if (!initialized) return false;
    
    if (!FindTargetWindow(windowTitle)) {
        return false;
    }

    try {
        // 获取GraphicsCaptureItem
        auto interop_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
        auto capture_interop = interop_factory.as<::IGraphicsCaptureItemInterop>();
        
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
        winrt::check_hresult(capture_interop->CreateForWindow(
            targetWindow,
            winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(item))
        ));

        captureItem = item;

        // 创建帧池
        framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            item.Size()
        );

        // 创建捕获会话
        captureSession = framePool.CreateCaptureSession(captureItem);
        captureSession.StartCapture();

        return true;
    }
    catch (winrt::hresult_error const& ex) {
        OutputDebugStringW(ex.message().c_str());
        return false;
    }
}

bool WindowCapture::FindTargetWindow(const std::wstring& windowTitle)
{
    targetWindow = FindWindowW(nullptr, windowTitle.c_str());
    return (targetWindow != nullptr);
}

bool WindowCapture::GetFrame(ID3D11Texture2D** texture)
{
    if (!initialized || !texture || !captureSession) return false;

    try {
        // 尝试获取下一帧
        auto frame = framePool.TryGetNextFrame();
        if (!frame) {
            return false;
        }

        // 获取帧的纹理
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        
        // 获取纹理描述
        D3D11_TEXTURE2D_DESC desc;
        frameTexture->GetDesc(&desc);

        // 创建目标纹理
        ID3D11Texture2D* destTexture = nullptr;
        {
            D3D11_TEXTURE2D_DESC destDesc = desc;
            destDesc.Usage = D3D11_USAGE_DEFAULT;
            destDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            destDesc.CPUAccessFlags = 0;
            destDesc.MiscFlags = 0;

            if (FAILED(d3dDevice->CreateTexture2D(&destDesc, nullptr, &destTexture))) {
                return false;
            }
        }

        // 复制纹理内容
        d3dContext->CopyResource(destTexture, frameTexture.get());

        *texture = destTexture;
        return true;
    }
    catch (winrt::hresult_error const& ex) {
        OutputDebugStringW(ex.message().c_str());
        return false;
    }
}

bool WindowCapture::CheckStagingTextureSize(UINT width, UINT height)
{
    if (!stagingTexture) return false;

    D3D11_TEXTURE2D_DESC desc;
    stagingTexture->GetDesc(&desc);
    return desc.Width == width && desc.Height == height;
}

void WindowCapture::Cleanup()
{
    if (captureSession) {
        captureSession.Close();
        captureSession = nullptr;
    }
    if (framePool) {
        framePool.Close();
        framePool = nullptr;
    }
    
    captureItem = nullptr;
    winrtDevice = nullptr;

    if (stagingTexture) {
        stagingTexture->Release();
        stagingTexture = nullptr;
    }
    if (d3dContext) {
        d3dContext->Release();
        d3dContext = nullptr;
    }
    if (d3dDevice) {
        d3dDevice->Release();
        d3dDevice = nullptr;
    }
    
    initialized = false;
} 