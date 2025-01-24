#pragma once
#include "win_config.hpp"
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.capture.interop.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <Shlwapi.h>
#include <wincodec.h>
#include "window_capturer.hpp"

class WindowUtils
{
public:
    // 分辨率结构体
    struct Resolution
    {
        int width;
        int height;
        UINT64 totalPixels;

        Resolution(int w = 0, int h = 0)
            : width(w), height(h), totalPixels(static_cast<UINT64>(w) * h) {}
    };

    // 功能检查
    static bool IsWindowsCaptureSupported()
    {
        try
        {
            return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
        }
        catch (...)
        {
            return false;
        }
    }

    // 窗口查找
    static std::vector<std::pair<HWND, std::wstring>> GetWindows();
    static HWND FindTargetWindow(const std::wstring &configuredTitle = L"");

    // 窗口操作
    static bool ResizeWindow(HWND hwnd, int width, int height, bool taskbarLower = true);

    // 辅助方法
    static std::wstring TrimRight(const std::wstring &str);

    // 分辨率计算
    static Resolution CalculateResolution(UINT64 totalPixels, double ratio);
    static Resolution CalculateResolutionByScreen(double targetRatio);

    // 截图相关函数
    static std::wstring GetScreenshotPath();
    static std::wstring GetGameScreenshotPath(HWND hwnd);
    static bool CaptureWindow(HWND hwnd, std::function<void(Microsoft::WRL::ComPtr<ID3D11Texture2D>)> callback, const RECT *cropRegion = nullptr);
    static bool SaveFrameToFile(ID3D11Texture2D *texture, const std::wstring &filePath);
    static HRESULT TextureToWICBitmap(ID3D11Texture2D *texture, Microsoft::WRL::ComPtr<IWICBitmapSource> &outBitmap);
    static bool SaveWICBitmapToFile(IWICBitmapSource *bitmap, const std::wstring &filePath);

    // 新增：会话控制方法
    static bool BeginCaptureSession(HWND hwnd, const RECT *cropRegion = nullptr);
    static void EndCaptureSession();
    static bool RequestNextFrame(std::function<void(Microsoft::WRL::ComPtr<ID3D11Texture2D>)> callback);
    static bool HasActiveSession();

    // D3D资源管理
    static bool EnsureD3DResources();
    static ID3D11Device *GetDevice() { return s_device.Get(); }
    static ID3D11DeviceContext *GetContext() { return s_context.Get(); }
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice GetWinRTDevice() { return s_winrtDevice; }

    // 缩放WIC位图到指定大小
    static Microsoft::WRL::ComPtr<IWICBitmapSource> ResizeWICBitmap(
        IWICBitmapSource *bitmap,
        UINT targetWidth,
        UINT targetHeight,
        WICBitmapInterpolationMode interpolationMode = WICBitmapInterpolationModeCubic);

    // 按长边缩放WIC位图
    static Microsoft::WRL::ComPtr<IWICBitmapSource> ResizeWICBitmapByLongEdge(
        IWICBitmapSource *bitmap,
        UINT longEdgeLength,
        WICBitmapInterpolationMode interpolationMode = WICBitmapInterpolationModeCubic);

    // 裁剪WIC位图
    static Microsoft::WRL::ComPtr<IWICBitmapSource> CropWICBitmap(
        IWICBitmapSource *source, int x, int y, int width, int height);

private:
    // 静态捕获器实例
    static std::unique_ptr<WindowCapturer> s_capturer;

    // 静态D3D资源
    static Microsoft::WRL::ComPtr<ID3D11Device> s_device;
    static Microsoft::WRL::ComPtr<ID3D11DeviceContext> s_context;
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice s_winrtDevice;
};