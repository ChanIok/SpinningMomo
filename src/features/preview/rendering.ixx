module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

export module Features.Preview.Rendering;

import std;
import Features.Preview.State;
import Core.State;
import Utils.Graphics.D3D;

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
auto render_frame(Core::State::AppState& state,
                  Microsoft::WRL::ComPtr<ID3D11Texture2D> capture_texture) -> void;

// 更新捕获纹理的SRV
auto update_capture_srv(Core::State::AppState& state,
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> std::expected<void, std::string>;

// 渲染基本的全屏四边形（显示捕获内容）
auto render_basic_quad(const Features::Preview::State::RenderingResources& resources) -> void;

// 渲染视口框
auto render_viewport_frame(Core::State::AppState& state,
                           const Features::Preview::State::RenderingResources& resources) -> void;

// 创建基本顶点缓冲区（全屏四边形）
auto create_basic_vertex_buffer(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11Buffer>, std::string>;

// 获取渲染资源（从状态中）
auto get_rendering_resources(Core::State::AppState& state)
    -> Features::Preview::State::RenderingResources*;

}  // namespace Features::Preview::Rendering