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

// 在帧池重建或关闭前解除绑定并释放缓存对捕获纹理的引用。
auto release_capture_surface(core::AppState& state) -> void;

// 调整渲染尺寸
auto resize_rendering(core::AppState& state, int width, int height)
    -> std::expected<void, std::string>;

// 按纹理对象身份复用单个 SRV，返回前解除绑定。
auto render_frame(core::AppState& state, ID3D11Texture2D* capture_texture)
    -> std::expected<void, std::string>;

}  // namespace features::preview::rendering
