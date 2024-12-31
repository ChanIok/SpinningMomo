#pragma once
#include "win_config.hpp"
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.capture.interop.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>

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

    // 窗口查找
    static HWND FindGameWindow();
    static std::vector<std::pair<HWND, std::wstring>> GetWindows();
    static bool CompareWindowTitle(const std::wstring& title1, const std::wstring& title2);
    
    // 窗口操作
    static bool ResizeWindow(HWND hwnd, int width, int height, bool topmost = false, bool taskbarLower = true);
    
    // 辅助方法
    static std::wstring TrimRight(const std::wstring& str);

    // 分辨率计算
    static Resolution CalculateResolution(UINT64 totalPixels, double ratio);
    static Resolution CalculateResolutionByScreen(double targetRatio);

    // 截图相关函数
    static bool CaptureWindow(HWND hwnd, const std::wstring& savePath);
    static std::wstring GetScreenshotPath();

private:
    // 回调函数
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    static winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForWindow(HWND hwnd);
    
    // 截图辅助函数
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateDirect3DDevice(ID3D11Device* d3dDevice);
    static bool SaveFrameToFile(ID3D11Texture2D* texture, const std::wstring& filePath);
    template<typename T>
    static Microsoft::WRL::ComPtr<T> GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object) {
        auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        Microsoft::WRL::ComPtr<T> result;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
        return result;
    }
}; 