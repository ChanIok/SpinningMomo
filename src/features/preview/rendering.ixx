module;

#include <wil/com.h>

export module Features.Preview.Rendering;

import std;
import Core.State;
import <d3d11.h>;
import <windows.h>;

export namespace Features::Preview::Rendering {

// 初始化渲染系统
auto initialize_rendering(Core::State::AppState& state, HWND hwnd, int width, int height)
    -> std::expected<void, std::string>;

// 清理渲染资源
auto cleanup_rendering(Core::State::AppState& state) -> void;

// 调整渲染尺寸
auto resize_rendering(Core::State::AppState& state, int width, int height)
    -> std::expected<void, std::string>;

// 渲染一帧
auto render_frame(Core::State::AppState& state, wil::com_ptr<ID3D11Texture2D> capture_texture)
    -> void;

}  // namespace Features::Preview::Rendering