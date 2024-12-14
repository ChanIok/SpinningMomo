#include "window_capture.h"
#include <dxgi1_2.h>
#include <d3d11.h>
#include <vector>

WindowCapture::WindowCapture()
    : targetWindow(nullptr)
    , d3dDevice(nullptr)
    , d3dContext(nullptr)
    , stagingTexture(nullptr)
    , initialized(false)
{
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
        &d3dContext                // 返回的设备���下文
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

bool WindowCapture::CaptureWindow(const std::wstring& windowTitle)
{
    if (!initialized) return false;
    
    if (!FindTargetWindow(windowTitle)) {
        return false;
    }

    return true;
}

bool WindowCapture::FindTargetWindow(const std::wstring& windowTitle)
{
    targetWindow = FindWindowW(nullptr, windowTitle.c_str());
    return (targetWindow != nullptr);
}

bool WindowCapture::GetFrame(ID3D11Texture2D** texture)
{
    if (!initialized || !texture || !targetWindow) return false;

    // 获取窗口大小
    RECT windowRect;
    if (!GetClientRect(targetWindow, &windowRect)) {
        return false;
    }

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    if (width <= 0 || height <= 0) {
        return false;
    }

    // 创建或更新暂存纹理
    if (!stagingTexture || !CheckStagingTextureSize(width, height)) {
        if (stagingTexture) {
            stagingTexture->Release();
            stagingTexture = nullptr;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(d3dDevice->CreateTexture2D(&desc, nullptr, &stagingTexture))) {
            return false;
        }
    }

    // 创建目标纹理
    ID3D11Texture2D* destTexture = nullptr;
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(d3dDevice->CreateTexture2D(&desc, nullptr, &destTexture))) {
            return false;
        }
    }

    // 捕获窗口内容
    HDC hdcWindow = GetDC(targetWindow);
    HDC hdcMemory = CreateCompatibleDC(hdcWindow);
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);

    // 获取窗口内容
    POINT ptClient = { 0, 0 };
    ClientToScreen(targetWindow, &ptClient);
    BitBlt(hdcMemory, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    // 获取位图数据
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // 负值表示从上到下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // 映射暂存纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(d3dContext->Map(stagingTexture, 0, D3D11_MAP_WRITE, 0, &mapped)))
    {
        // 从位图获取像素数据到暂存纹理
        GetDIBits(hdcMemory, hBitmap, 0, height, mapped.pData, &bmi, DIB_RGB_COLORS);
        d3dContext->Unmap(stagingTexture, 0);

        // 复制到目标纹理
        d3dContext->CopyResource(destTexture, stagingTexture);
    }

    // 清理GDI资源
    SelectObject(hdcMemory, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMemory);
    ReleaseDC(targetWindow, hdcWindow);

    *texture = destTexture;
    return true;
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