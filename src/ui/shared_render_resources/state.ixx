module;

export module UI.SharedRenderResources.State;

import <d2d1_3.h>;
import <d3d11.h>;
import <dwrite_3.h>;
import <wil/com.h>;

namespace UI::SharedRenderResources::State {

// 共享状态只持有设备级资源。
// 浮窗、上下文菜单各自再创建自己的 device context / swap chain / composition surface。
export struct SharedRenderResourcesState {
  wil::com_ptr<ID3D11Device> d3d_device;
  wil::com_ptr<ID3D11DeviceContext> d3d_device_context;
  wil::com_ptr<ID2D1Factory7> d2d_factory;
  wil::com_ptr<ID2D1Device> d2d_device;
  wil::com_ptr<IDWriteFactory7> write_factory;

  bool is_initialized = false;
};

}  // namespace UI::SharedRenderResources::State
