#include "ui/shared_render_resources/shared_render_resources.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/dwrite_3.hpp"
#include "vendor/windows/dxgi1_2.hpp"

#include "core/state/app_state.hpp"
#include "ui/shared_render_resources/state.hpp"

namespace ui::shared_render_resources {

auto create_factories(SharedRenderResourcesState& shared) -> bool {
  const HRESULT factory_hr =
      D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7), nullptr,
                        reinterpret_cast<void**>(shared.d2d_factory.put()));
  if (FAILED(factory_hr) || !shared.d2d_factory) {
    return false;
  }

  const HRESULT write_hr =
      DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                          reinterpret_cast<IUnknown**>(shared.write_factory.put()));
  return SUCCEEDED(write_hr) && shared.write_factory;
}

auto create_d3d_device(SharedRenderResourcesState& shared) -> bool {
  UINT create_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                     create_device_flags, nullptr, 0, D3D11_SDK_VERSION,
                                     shared.d3d_device.put(), nullptr,
                                     shared.d3d_device_context.put())) &&
         shared.d3d_device && shared.d3d_device_context;
}

auto create_d2d_device(SharedRenderResourcesState& shared) -> bool {
  wil::com_ptr<IDXGIDevice> dxgi_device;
  if (FAILED(shared.d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()))) || !dxgi_device) {
    return false;
  }

  return SUCCEEDED(shared.d2d_factory->CreateDevice(dxgi_device.get(), shared.d2d_device.put())) &&
         shared.d2d_device;
}

auto ensure_initialized(core::AppState& state) -> bool {
  auto& shared = *state.shared_render_resources;
  if (shared.is_initialized) {
    return true;
  }

  cleanup(state);

  // 共享层只关心设备级对象；任何窗口级资源都留在各自模块内部创建和销毁。
  if (!create_factories(shared) || !create_d3d_device(shared) || !create_d2d_device(shared)) {
    cleanup(state);
    return false;
  }

  shared.is_initialized = true;
  return true;
}

auto cleanup(core::AppState& state) -> void {
  auto& shared = *state.shared_render_resources;
  shared.d2d_device.reset();
  shared.write_factory.reset();
  shared.d2d_factory.reset();
  shared.d3d_device_context.reset();
  shared.d3d_device.reset();
  shared.is_initialized = false;
}

}  // namespace ui::shared_render_resources
