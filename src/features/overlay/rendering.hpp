#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"

#include "core/state/app_state.hpp"
#include "features/overlay/state.hpp"

namespace features::overlay::rendering {

// 初始化渲染系统
auto initialize_rendering(core::AppState& state) -> std::expected<void, std::string>;

// 调整交换链大小
auto resize_rendering(core::AppState& state) -> std::expected<void, std::string>;

// 渲染帧
auto render_frame(core::AppState& state, wil::com_ptr<ID3D11Texture2D> frame_texture) -> void;

// 清理渲染资源
auto cleanup_rendering(core::AppState& state) -> void;

}  // namespace features::overlay::rendering
