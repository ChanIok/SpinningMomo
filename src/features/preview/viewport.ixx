module;

#include <d3d11.h>
#include <wrl/client.h>

export module Features.Preview.Viewport;

import std;
import Core.State;

export namespace Features::Preview::Viewport {

// 更新视口矩形状态
auto update_viewport_rect(Core::State::AppState& state) -> void;

// 渲染视口框到屏幕
auto render_viewport_frame(Core::State::AppState& state, ID3D11DeviceContext* context,
                           const Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertex_shader,
                           const Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixel_shader,
                           const Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout) -> void;

}  // namespace Features::Preview::Viewport