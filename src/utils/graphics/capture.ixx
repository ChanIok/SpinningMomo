module;

// #include <d3d11.h>
// #include <wil/com.h>
// #include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

export module Utils.Graphics.Capture;

import std;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Graphics::Capture {

// 捕获会话
export struct CaptureSession {
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem capture_item{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool{nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{nullptr};
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device{nullptr};
  winrt::event_token frame_token;
  bool need_hide_cursor = false;
};

// 捕获会话配置
export struct CaptureSessionOptions {
  bool capture_cursor = false;
  bool border_required = false;
};

export using Direct3D11CaptureFrame = winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame;

// 帧回调函数类型
using FrameCallback = std::function<void(Direct3D11CaptureFrame)>;

// 创建WinRT设备
export auto create_winrt_device(ID3D11Device* d3d_device)
    -> std::expected<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice, std::string>;

// 检测 WGC 会话属性支持能力
export auto is_cursor_capture_control_supported() -> bool;
export auto is_border_control_supported() -> bool;

// 创建捕获会话
export auto create_capture_session(
    HWND target_window,
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice& device, int width,
    int height, FrameCallback frame_callback, int frame_pool_size = 1,
    const CaptureSessionOptions& options = {}) -> std::expected<CaptureSession, std::string>;

// 开始捕获
export auto start_capture(CaptureSession& session) -> std::expected<void, std::string>;

// 停止捕获
export auto stop_capture(CaptureSession& session) -> void;

// 清理捕获资源
export auto cleanup_capture_session(CaptureSession& session) -> void;

// 重建帧池
export auto recreate_frame_pool(CaptureSession& session, int width, int height) -> void;

// 从WinRT对象获取DXGI接口的辅助函数
export template <typename T>
auto get_dxgi_interface_from_object(const winrt::Windows::Foundation::IInspectable& object)
    -> wil::com_ptr<T>;

}  // namespace Utils::Graphics::Capture
