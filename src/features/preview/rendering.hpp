#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"

#include "core/state/app_state.hpp"

namespace features::preview::rendering {

// 初始化渲染系统
auto initialize_rendering(core::AppState& state, HWND hwnd, int width, int height)
    -> std::expected<void, std::string>;

// 清理渲染资源
auto cleanup_rendering(core::AppState& state) -> void;

// 调整渲染尺寸
auto resize_rendering(core::AppState& state, int width, int height)
    -> std::expected<void, std::string>;

// 在重建 WGC 帧池前解除对旧捕获 surface 的绑定与持有。
auto release_capture_surface(core::AppState& state) -> void;

// 渲染一帧
auto render_frame(core::AppState& state, wil::com_ptr<ID3D11Texture2D> capture_texture) -> void;

}  // namespace features::preview::rendering
