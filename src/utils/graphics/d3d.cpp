module;

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <windows.h>
#include <wrl/client.h>

#include <iostream>

module Utils.Graphics.D3D;

import std;
import Utils.Logger;

namespace Utils::Graphics::D3D {

auto create_d3d_context(HWND hwnd, int width, int height)
    -> std::expected<D3DContext, std::string> {
  D3DContext context;

  // 创建交换链描述
  DXGI_SWAP_CHAIN_DESC scd = {};
  scd.BufferCount = 2;
  scd.BufferDesc.Width = width;
  scd.BufferDesc.Height = height;
  scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  scd.BufferDesc.RefreshRate.Numerator = 0;
  scd.BufferDesc.RefreshRate.Denominator = 1;
  scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scd.OutputWindow = hwnd;
  scd.SampleDesc.Count = 1;
  scd.SampleDesc.Quality = 0;
  scd.Windowed = TRUE;
  scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  // 创建设备和交换链
  D3D_FEATURE_LEVEL featureLevel;
  UINT createDeviceFlags = 0;
#ifdef _DEBUG
  createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION,
      &scd, &context.swap_chain, &context.device, &featureLevel, &context.context);

  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create D3D device and swap chain, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 创建渲染目标
  if (auto result = create_render_target(context); !result) {
    return std::unexpected(result.error());
  }

  return context;
}

auto create_render_target(D3DContext& context) -> std::expected<void, std::string> {
  // 获取后缓冲
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
  HRESULT hr = context.swap_chain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  if (FAILED(hr)) {
    auto error_msg =
        std::format("Failed to get back buffer, HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 创建渲染目标视图
  hr = context.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &context.render_target);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create render target view, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return {};
}

auto compile_shader(const std::string& shader_code, const std::string& entry_point,
                    const std::string& target)
    -> std::expected<Microsoft::WRL::ComPtr<ID3DBlob>, std::string> {
  Microsoft::WRL::ComPtr<ID3DBlob> blob;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

  UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
  compileFlags |= D3DCOMPILE_DEBUG;
#endif

  HRESULT hr = D3DCompile(shader_code.c_str(), shader_code.length(), nullptr, nullptr, nullptr,
                          entry_point.c_str(), target.c_str(), compileFlags, 0, &blob, &errorBlob);

  if (FAILED(hr)) {
    std::string error_msg =
        std::format("Shader compilation failed, HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
    if (errorBlob) {
      std::string compiler_error(static_cast<char*>(errorBlob->GetBufferPointer()),
                                 errorBlob->GetBufferSize());
      error_msg += std::format(" - Compiler error: {}", compiler_error);
    }
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return blob;
}

auto create_basic_shader_resources(ID3D11Device* device, const std::string& vertex_code,
                                   const std::string& pixel_code)
    -> std::expected<ShaderResources, std::string> {
  ShaderResources resources;

  // 编译顶点着色器
  auto vs_result = compile_shader(vertex_code, "main", "vs_4_0");
  if (!vs_result) {
    return std::unexpected(vs_result.error());
  }
  auto vsBlob = vs_result.value();

  // 编译像素着色器
  auto ps_result = compile_shader(pixel_code, "main", "ps_4_0");
  if (!ps_result) {
    return std::unexpected(ps_result.error());
  }
  auto psBlob = ps_result.value();

  // 创建着色器
  HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &resources.vertex_shader);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create vertex shader, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                 &resources.pixel_shader);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create pixel shader, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  // 创建输入布局（基本的位置+纹理坐标）
  D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};

  hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                 &resources.input_layout);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create input layout, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  // 创建基本的采样器和混合状态
  auto sampler_result = create_linear_sampler(device);
  if (!sampler_result) {
    return std::unexpected(sampler_result.error());
  }
  resources.sampler = sampler_result.value();

  auto blend_result = create_alpha_blend_state(device);
  if (!blend_result) {
    return std::unexpected(blend_result.error());
  }
  resources.blend_state = blend_result.value();

  return resources;
}

auto create_viewport_shader_resources(ID3D11Device* device, const std::string& vertex_code,
                                      const std::string& pixel_code)
    -> std::expected<ShaderResources, std::string> {
  ShaderResources resources;

  // 编译着色器
  auto vs_result = compile_shader(vertex_code, "main", "vs_4_0");
  if (!vs_result) {
    return std::unexpected(vs_result.error());
  }
  auto vsBlob = vs_result.value();

  auto ps_result = compile_shader(pixel_code, "main", "ps_4_0");
  if (!ps_result) {
    return std::unexpected(ps_result.error());
  }
  auto psBlob = ps_result.value();

  // 创建着色器
  HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &resources.vertex_shader);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create vertex shader, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                 &resources.pixel_shader);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create pixel shader, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  // 创建视口框的输入布局（位置+颜色）
  D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};

  hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                 &resources.input_layout);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create input layout, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  return resources;
}

auto create_vertex_buffer(ID3D11Device* device, const void* vertices, size_t vertex_count,
                          size_t vertex_size, bool dynamic)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11Buffer>, std::string> {
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
  bd.ByteWidth = static_cast<UINT>(vertex_count * vertex_size);
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

  D3D11_SUBRESOURCE_DATA initData = {};
  initData.pSysMem = vertices;

  Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
  HRESULT hr = device->CreateBuffer(&bd, vertices ? &initData : nullptr, &buffer);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create vertex buffer, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return buffer;
}

auto create_linear_sampler(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11SamplerState>, std::string> {
  D3D11_SAMPLER_DESC samplerDesc = {};
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  samplerDesc.MinLOD = 0;
  samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
  HRESULT hr = device->CreateSamplerState(&samplerDesc, &sampler);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create sampler state, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  return sampler;
}

auto create_alpha_blend_state(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11BlendState>, std::string> {
  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
  HRESULT hr = device->CreateBlendState(&blendDesc, &blendState);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create blend state, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    return std::unexpected(error_msg);
  }

  return blendState;
}

auto resize_swap_chain(D3DContext& context, int width, int height)
    -> std::expected<void, std::string> {
  // 释放渲染目标
  context.render_target.Reset();

  // 调整交换链大小
  HRESULT hr = context.swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to resize swap chain, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  // 重新创建渲染目标
  return create_render_target(context);
}

auto cleanup_d3d_context(D3DContext& context) -> void {
  if (context.context) {
    context.context->ClearState();
    context.context->Flush();
  }

  context.render_target.Reset();
  context.swap_chain.Reset();
  context.context.Reset();
  context.device.Reset();
}

auto cleanup_shader_resources(ShaderResources& resources) -> void {
  resources.vertex_shader.Reset();
  resources.pixel_shader.Reset();
  resources.input_layout.Reset();
  resources.vertex_buffer.Reset();
  resources.sampler.Reset();
  resources.blend_state.Reset();
}

}  // namespace Utils::Graphics::D3D