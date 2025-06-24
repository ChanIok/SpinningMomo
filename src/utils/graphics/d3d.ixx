module;

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <windows.h>
#include <wrl/client.h>

export module Utils.Graphics.D3D;

import std;

namespace Utils::Graphics::D3D {

// D3D设备上下文
export struct D3DContext {
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
};

// 着色器资源
export struct ShaderResources {
  Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
  Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
  Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
};

// 创建D3D设备和交换链
export auto create_d3d_context(HWND hwnd, int width, int height)
    -> std::expected<D3DContext, std::string>;

// 创建渲染目标
export auto create_render_target(D3DContext& context) -> std::expected<void, std::string>;

// 编译着色器
export auto compile_shader(const std::string& shader_code, const std::string& entry_point,
                           const std::string& target)
    -> std::expected<Microsoft::WRL::ComPtr<ID3DBlob>, std::string>;

// 创建基本的着色器资源
export auto create_basic_shader_resources(ID3D11Device* device, const std::string& vertex_code,
                                          const std::string& pixel_code)
    -> std::expected<ShaderResources, std::string>;

// 创建视口框着色器资源
export auto create_viewport_shader_resources(ID3D11Device* device, const std::string& vertex_code,
                                             const std::string& pixel_code)
    -> std::expected<ShaderResources, std::string>;

// 创建顶点缓冲区
export auto create_vertex_buffer(ID3D11Device* device, const void* vertices, size_t vertex_count,
                                 size_t vertex_size, bool dynamic = false)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11Buffer>, std::string>;

// 创建采样器状态
export auto create_linear_sampler(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11SamplerState>, std::string>;

// 创建混合状态
export auto create_alpha_blend_state(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11BlendState>, std::string>;

// 调整交换链大小
export auto resize_swap_chain(D3DContext& context, int width, int height)
    -> std::expected<void, std::string>;

// 清理D3D上下文
export auto cleanup_d3d_context(D3DContext& context) -> void;

// 清理着色器资源
export auto cleanup_shader_resources(ShaderResources& resources) -> void;

}  // namespace Utils::Graphics::D3D