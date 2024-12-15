#include "capture_test.h"
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <memory>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
}

// Helper function to create IDirect3DDevice
static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateD3DDevice(winrt::com_ptr<ID3D11Device> const& d3dDevice)
{
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));
    
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), reinterpret_cast<::IInspectable**>(winrt::put_abi(device))));
    
    return device;
}

// Helper function to get DXGI interface
template<typename T>
static winrt::com_ptr<T> GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
    return result;
}

// 保存纹理到文件
static bool SaveTextureToFile(winrt::com_ptr<ID3D11Device> const& device, 
                     winrt::com_ptr<ID3D11DeviceContext> const& context,
                     winrt::com_ptr<ID3D11Texture2D> const& texture,
                     const std::wstring& filename)
{
    try {
        // 获取纹理描述
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        // 创建可以CPU访问的纹理
        D3D11_TEXTURE2D_DESC copyDesc = desc;
        copyDesc.Usage = D3D11_USAGE_STAGING;
        copyDesc.BindFlags = 0;
        copyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        copyDesc.MiscFlags = 0;

        winrt::com_ptr<ID3D11Texture2D> stagingTexture;
        winrt::check_hresult(device->CreateTexture2D(&copyDesc, nullptr, stagingTexture.put()));

        // 复制纹理内容
        context->CopyResource(stagingTexture.get(), texture.get());

        // 初始化 WIC
        winrt::com_ptr<IWICImagingFactory> factory;
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

        // 创建文件
        winrt::com_ptr<IWICBitmapEncoder> encoder;
        winrt::check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));

        winrt::com_ptr<IStream> stream;
        winrt::check_hresult(SHCreateStreamOnFileW(filename.c_str(), STGM_CREATE | STGM_WRITE, stream.put()));

        winrt::check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

        winrt::com_ptr<IWICBitmapFrameEncode> frame;
        winrt::check_hresult(encoder->CreateNewFrame(frame.put(), nullptr));

        winrt::check_hresult(frame->Initialize(nullptr));

        // 设置大小
        winrt::check_hresult(frame->SetSize(desc.Width, desc.Height));

        // 设置像素格式
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        winrt::check_hresult(frame->SetPixelFormat(&format));

        // 映射纹理
        D3D11_MAPPED_SUBRESOURCE mapped;
        winrt::check_hresult(context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped));

        // 写入像素数据
        winrt::check_hresult(frame->WritePixels(desc.Height, mapped.RowPitch, mapped.RowPitch * desc.Height, 
            static_cast<BYTE*>(mapped.pData)));

        // 解除映射
        context->Unmap(stagingTexture.get(), 0);

        // 提交帧和编码器
        winrt::check_hresult(frame->Commit());
        winrt::check_hresult(encoder->Commit());

        return true;
    }
    catch (winrt::hresult_error const& ex) {
        OutputDebugStringW(ex.message().c_str());
        return false;
    }
}

bool CaptureWindowToFile(HWND window, const std::wstring& filename)
{
    try
    {
        // 初始化 COM 和 WinRT
        winrt::apartment_context context;

        // 创建 D3D 设备
        winrt::com_ptr<ID3D11Device> d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext> d3dContext;
        D3D_FEATURE_LEVEL featureLevel;
        
        UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        winrt::check_hresult(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            nullptr, 0,
            D3D11_SDK_VERSION,
            d3dDevice.put(),
            &featureLevel,
            d3dContext.put()
        ));

        // 获取 GraphicsCaptureItem
        auto interop_factory = winrt::get_activation_factory<winrt::GraphicsCaptureItem>();
        auto capture_interop = interop_factory.as<IGraphicsCaptureItemInterop>();
        winrt::GraphicsCaptureItem item{ nullptr };
        winrt::check_hresult(capture_interop->CreateForWindow(
            window,
            winrt::guid_of<winrt::GraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(item))
        ));

        // 创建帧池
        auto framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
            CreateD3DDevice(d3dDevice),
            winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            item.Size()
        );

        // 创建捕获会话
        auto session = framePool.CreateCaptureSession(item);
        session.StartCapture();

        // 等待一帧
        bool frameCaptured = false;
        framePool.FrameArrived([&](auto&& sender, auto&&)
        {
            if (!frameCaptured)
            {
                auto frame = sender.TryGetNextFrame();
                auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
                
                if (SaveTextureToFile(d3dDevice, d3dContext, frameTexture, filename))
                {
                    OutputDebugStringW(L"Screenshot saved successfully\n");
                }
                else
                {
                    OutputDebugStringW(L"Failed to save screenshot\n");
                }
                
                frameCaptured = true;
            }
        });

        // 等待捕获完成
        while (!frameCaptured)
        {
            Sleep(10);
        }

        // 清理资源
        session.Close();
        framePool.Close();

        return true;
    }
    catch (winrt::hresult_error const& ex)
    {
        OutputDebugStringW(ex.message().c_str());
        return false;
    }
}

void CaptureTest()
{
    // 获取当前窗口句柄
    HWND hwnd = GetForegroundWindow();
    std::wstring filename = L"screenshot.png";
    
    if (CaptureWindowToFile(hwnd, filename))
    {
        MessageBoxW(NULL, L"截图已保存到 screenshot.png", L"成功", MB_OK);
    }
    else
    {
        MessageBoxW(NULL, L"截图保存失败", L"错误", MB_OK);
    }
} 