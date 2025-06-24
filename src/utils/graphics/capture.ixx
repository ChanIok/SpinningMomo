module;

#include <d3d11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <wrl/client.h>

export module Utils.Graphics.Capture;

import std;

namespace Utils::Graphics::Capture {

// 捕获会话
export struct CaptureSession {
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem capture_item{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool{nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{nullptr};
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device{nullptr};
  winrt::event_token frame_token;
};

// 帧回调函数类型
using FrameCallback = std::function<void(Microsoft::WRL::ComPtr<ID3D11Texture2D>)>;

// 检查系统是否支持Graphics Capture
export auto is_capture_supported() -> bool;

// 创建WinRT设备
export auto create_winrt_device(ID3D11Device* d3d_device)
    -> std::expected<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice, std::string>;

// 创建捕获会话
export auto create_capture_session(
    HWND target_window,
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice& device, int width,
    int height, FrameCallback frame_callback) -> std::expected<CaptureSession, std::string>;

// 开始捕获
export auto start_capture(CaptureSession& session) -> std::expected<void, std::string>;

// 停止捕获
export auto stop_capture(CaptureSession& session) -> void;

// 清理捕获资源
export auto cleanup_capture_session(CaptureSession& session) -> void;

// 从WinRT对象获取DXGI接口的辅助函数
export template <typename T>
auto get_dxgi_interface_from_object(const winrt::Windows::Foundation::IInspectable& object)
    -> Microsoft::WRL::ComPtr<T>;

}  // namespace Utils::Graphics::Capture