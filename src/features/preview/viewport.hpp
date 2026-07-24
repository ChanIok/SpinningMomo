#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"

#include "core/state/app_state.hpp"

namespace features::preview::viewport {

// 更新视口矩形状态
auto update_viewport_rect(core::AppState& state) -> void;

// 渲染视口框到屏幕
auto render_viewport_frame(core::AppState& state, ID3D11DeviceContext* context,
                           const wil::com_ptr<ID3D11VertexShader>& vertex_shader,
                           const wil::com_ptr<ID3D11PixelShader>& pixel_shader,
                           const wil::com_ptr<ID3D11InputLayout>& input_layout) -> void;

}  // namespace features::preview::viewport
