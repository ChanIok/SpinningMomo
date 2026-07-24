#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/dwrite_3.hpp"

namespace ui::shared_render_resources {

// 共享状态只持有设备级资源。
// 浮窗、上下文菜单各自再创建自己的 device context / swap chain / composition surface。
struct SharedRenderResourcesState {
  wil::com_ptr<ID3D11Device> d3d_device;
  wil::com_ptr<ID3D11DeviceContext> d3d_device_context;
  wil::com_ptr<ID2D1Factory7> d2d_factory;
  wil::com_ptr<ID2D1Device> d2d_device;
  wil::com_ptr<IDWriteFactory7> write_factory;

  bool is_initialized = false;
};

}  // namespace ui::shared_render_resources
