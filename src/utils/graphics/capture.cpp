module;

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

module Utils.Graphics.Capture;

import std;
import Utils.Logger;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Graphics::Capture {

auto create_winrt_device(ID3D11Device* d3d_device)
    -> std::expected<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice, std::string> {
  if (!d3d_device) {
    return std::unexpected("D3D device is null");
  }

  // 获取DXGI设备接口
  wil::com_ptr<IDXGIDevice> dxgi_device;
  HRESULT hr = d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()));
  if (FAILED(hr)) {
    auto error_msg =
        std::format("Failed to get DXGI device, HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 创建WinRT设备
  winrt::com_ptr<::IInspectable> inspectable;
  hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put());
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create WinRT device, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  auto winrt_device =
      inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
  if (!winrt_device) {
    auto error_msg = "Failed to get WinRT Direct3D device interface";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return winrt_device;
}

auto create_capture_session(
    HWND target_window,
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice& device, int width,
    int height, FrameCallback frame_callback, int frame_pool_size,
    const CaptureSessionOptions& options) -> std::expected<CaptureSession, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Target window is invalid");
  }

  if (!frame_callback) {
    return std::unexpected("Frame callback is null");
  }

  CaptureSession session;
  session.winrt_device = device;

  // 创建捕获项
  auto interop =
      winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
                                    IGraphicsCaptureItemInterop>();
  HRESULT hr = interop->CreateForWindow(
      target_window, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
      reinterpret_cast<void**>(winrt::put_abi(session.capture_item)));

  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create capture item, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 创建帧池
  session.frame_pool =
      winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
          device, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
          frame_pool_size, {width, height});

  if (!session.frame_pool) {
    auto error_msg = "Failed to create frame pool";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 设置帧到达回调
  session.frame_token = session.frame_pool.FrameArrived([frame_callback](auto&& sender, auto&&) {
    if (auto frame = sender.TryGetNextFrame()) {
      frame_callback(frame);
    }
  });

  // 创建捕获会话
  session.session = session.frame_pool.CreateCaptureSession(session.capture_item);
  if (!session.session) {
    auto error_msg = "Failed to create capture session";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 尝试禁用光标捕获（如果支持）
  if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
          winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
          L"IsCursorCaptureEnabled")) {
    session.session.IsCursorCaptureEnabled(options.capture_cursor);
  } else if (!options.capture_cursor) {
    // 如果不支持 IsCursorCaptureEnabled，且要求隐藏光标，则标记需要手动隐藏光标
    session.need_hide_cursor = true;
    Logger().warn(
        "IsCursorCaptureEnabled is not available, cursor visibility cannot be controlled "
        "without manual fallback");
  }

  // 尝试禁用边框（如果支持）
  if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
          winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
          L"IsBorderRequired")) {
    session.session.IsBorderRequired(options.border_required);
  }

  return session;
}

auto start_capture(CaptureSession& session) -> std::expected<void, std::string> {
  if (!session.session) {
    return std::unexpected("Capture session is null");
  }

  try {
    session.session.StartCapture();
    return {};
  } catch (const winrt::hresult_error& e) {
    auto error_msg = std::format("WinRT error occurred while starting capture: {}",
                                 winrt::to_string(e.message()));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  } catch (...) {
    auto error_msg = "Unknown error occurred while starting capture";
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }
}

auto stop_capture(CaptureSession& session) -> void {
  if (session.session) {
    session.session.Close();
    session.session = nullptr;
  }

  if (session.frame_pool) {
    session.frame_pool.FrameArrived(session.frame_token);
    session.frame_pool.Close();
    session.frame_pool = nullptr;
  }

  session.capture_item = nullptr;
}

auto cleanup_capture_session(CaptureSession& session) -> void {
  stop_capture(session);

  session.winrt_device = nullptr;
}

auto recreate_frame_pool(CaptureSession& session, int width, int height) -> void {
  if (session.frame_pool) {
    session.frame_pool.Recreate(
        session.winrt_device,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 1,
        {width, height});
  }
}

template <typename T>
auto get_dxgi_interface_from_object(const winrt::Windows::Foundation::IInspectable& object)
    -> wil::com_ptr<T> {
  auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  wil::com_ptr<T> result;
  winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(result.put())));
  return result;
}

// 显式实例化模板函数
template auto get_dxgi_interface_from_object<ID3D11Texture2D>(
    const winrt::Windows::Foundation::IInspectable&) -> wil::com_ptr<ID3D11Texture2D>;

}  // namespace Utils::Graphics::Capture
