#pragma once
#include <windows.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.capture.interop.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <Shlwapi.h>
#include "window_capturer.hpp"

class WindowUtils {
public:
    // 分辨率结构体
    struct Resolution {
        int width;
        int height;
        UINT64 totalPixels;

        Resolution(int w = 0, int h = 0) 
            : width(w), height(h), totalPixels(static_cast<UINT64>(w) * h) {}
    };

    // 功能检查
    static bool IsWindowsCaptureSupported() {
        try {
            // Windows 10 1803 (Build 17134)
            return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
        }
        catch (...) {
            return false;
        }
    }

    // 窗口查找
    static std::vector<std::pair<HWND, std::wstring>> GetWindows();
    static HWND FindTargetWindow(const std::wstring& configuredTitle = L"");
    
    // 窗口操作
    static bool ResizeWindow(HWND hwnd, int width, int height, bool taskbarLower = true, bool activate = true);
    
    // 分辨率计算
    static Resolution CalculateResolution(UINT64 totalPixels, double ratio);
    static Resolution CalculateResolutionByScreen(double targetRatio);

    static bool TakeScreenshotAsync(
        HWND hwnd, 
        const std::wstring& filePath, 
        std::function<void(bool success, const std::wstring& path)> completionCallback = nullptr
    );

    // 截图相关函数
    static std::wstring GetScreenshotPath();
    static std::wstring GetGameScreenshotPath(HWND hwnd);
    static bool SaveFrameToFile(ID3D11Texture2D* texture, const std::wstring& filePath);
    static void CleanupCaptureResources();

    // D3D资源管理
    static bool EnsureD3DResources();
    static ID3D11Device* GetDevice() { return s_device.Get(); }
    static ID3D11DeviceContext* GetContext() { return s_context.Get(); }
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice GetWinRTDevice() { return s_winrtDevice; }

    // 鼠标显示控制
    static void HideCursor();
    static void ShowCursor();
    
    // 窗口边框控制
    static bool ToggleWindowBorder(HWND hwnd);

private:
    // 静态捕获器实例
    static std::unique_ptr<WindowCapturer> s_capturer;

    // 静态D3D资源
    static Microsoft::WRL::ComPtr<ID3D11Device> s_device;
    static Microsoft::WRL::ComPtr<ID3D11DeviceContext> s_context;
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice s_winrtDevice;

    static bool s_cursorHidden;  // 标记鼠标是否被我们隐藏
}; 