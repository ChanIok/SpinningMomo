module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <iostream>

module Features.Preview.Rendering;

import std;
import Core.State;
import Utils.Graphics.D3D;
import Utils.Logger;
import Features.Preview.State;
import Features.Preview.Types;
import Features.Preview.Shaders;
import Features.Preview.Viewport;

namespace Features::Preview::Rendering {

auto create_basic_vertex_buffer(ID3D11Device* device)
    -> std::expected<Microsoft::WRL::ComPtr<ID3D11Buffer>, std::string> {
  // 创建全屏四边形的顶点数据
  Features::Preview::Types::Vertex vertices[] = {
      {-1.0f, 1.0f, 0.0f, 0.0f},   // 左上
      {1.0f, 1.0f, 1.0f, 0.0f},    // 右上
      {-1.0f, -1.0f, 0.0f, 1.0f},  // 左下
      {1.0f, -1.0f, 1.0f, 1.0f}    // 右下
  };

  auto buffer_result = Utils::Graphics::D3D::create_vertex_buffer(
      device, vertices, 4, sizeof(Features::Preview::Types::Vertex));

  if (!buffer_result) {
    return std::unexpected("Failed to create vertex buffer");
  }

  return buffer_result.value();
}

auto initialize_rendering(Core::State::AppState& state, HWND hwnd, int width, int height)
    -> std::expected<void, std::string> {
  auto& resources = state.preview->rendering_resources;

  // 创建D3D上下文
  auto d3d_result = Utils::Graphics::D3D::create_d3d_context(hwnd, width, height);
  if (!d3d_result) {
    Logger().error("Failed to create D3D context for preview rendering: {}", d3d_result.error());
    return std::unexpected(d3d_result.error().find("device") != std::string::npos
                               ? "Failed to initialize D3D device"
                               : "Failed to create D3D resources");
  }
  resources.d3d_context = std::move(d3d_result.value());

  // 创建基本渲染着色器
  auto basic_shader_result = Utils::Graphics::D3D::create_basic_shader_resources(
      resources.d3d_context.device.Get(), Features::Preview::Shaders::BASIC_VERTEX_SHADER,
      Features::Preview::Shaders::BASIC_PIXEL_SHADER);

  if (!basic_shader_result) {
    Logger().error("Failed to create basic shader resources");
    return std::unexpected("Failed to compile basic shaders");
  }
  resources.basic_shaders = std::move(basic_shader_result.value());

  // 创建视口框着色器
  auto viewport_shader_result = Utils::Graphics::D3D::create_viewport_shader_resources(
      resources.d3d_context.device.Get(), Features::Preview::Shaders::VIEWPORT_VERTEX_SHADER,
      Features::Preview::Shaders::VIEWPORT_PIXEL_SHADER);

  if (!viewport_shader_result) {
    Logger().error("Failed to create viewport shader resources");
    return std::unexpected("Failed to compile viewport shaders");
  }
  resources.viewport_shaders = std::move(viewport_shader_result.value());

  // 创建基本顶点缓冲区（全屏四边形）
  auto vertex_buffer_result = create_basic_vertex_buffer(resources.d3d_context.device.Get());
  if (!vertex_buffer_result) {
    return std::unexpected(vertex_buffer_result.error());
  }
  resources.basic_vertex_buffer = std::move(vertex_buffer_result.value());

  resources.initialized = true;
  state.preview->d3d_initialized = true;

  Logger().info("Preview rendering system initialized successfully");
  return {};
}

auto cleanup_rendering(Core::State::AppState& state) -> void {
  auto& resources = state.preview->rendering_resources;

  if (resources.initialized) {
    // 清理着色器资源
    Utils::Graphics::D3D::cleanup_shader_resources(resources.basic_shaders);
    Utils::Graphics::D3D::cleanup_shader_resources(resources.viewport_shaders);

    // 清理D3D上下文
    Utils::Graphics::D3D::cleanup_d3d_context(resources.d3d_context);

    // 重置资源
    resources.capture_srv.Reset();
    resources.basic_vertex_buffer.Reset();
    resources.viewport_vertex_buffer.Reset();
    resources.initialized = false;
  }

  state.preview->d3d_initialized = false;
  Logger().info("Preview rendering resources cleaned up");
}

auto resize_rendering(Core::State::AppState& state, int width, int height)
    -> std::expected<void, std::string> {
  if (!state.preview->rendering_resources.initialized) {
    return std::unexpected("D3D not initialized");
  }

  auto& resources = state.preview->rendering_resources;

  resources.resources_busy.store(true, std::memory_order_release);

  // 调整交换链大小
  auto resize_result =
      Utils::Graphics::D3D::resize_swap_chain(resources.d3d_context, width, height);

  resources.resources_busy.store(false, std::memory_order_release);

  if (!resize_result) {
    Logger().error("Failed to resize swap chain");
    return std::unexpected("Failed to resize swap chain");
  }

  Logger().debug("Preview rendering resized to {}x{}", width, height);
  return {};
}

auto update_capture_srv(Core::State::AppState& state,
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> std::expected<void, std::string> {
  if (!state.preview->rendering_resources.initialized || !texture) {
    return std::unexpected("Invalid rendering resources or texture");
  }

  auto& resources = state.preview->rendering_resources;

  // 获取纹理描述
  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);

  // 创建着色器资源视图描述
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = desc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.MipLevels = 1;

  // 创建新的SRV
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newSRV;
  HRESULT hr =
      resources.d3d_context.device->CreateShaderResourceView(texture.Get(), &srvDesc, &newSRV);
  if (FAILED(hr)) {
    Logger().error("Failed to create shader resource view, HRESULT: 0x{:08X}",
                   static_cast<unsigned int>(hr));
    return std::unexpected("Failed to create shader resource view");
  }

  resources.capture_srv = newSRV;
  return {};
}

auto render_basic_quad(const Features::Preview::Types::RenderingResources& resources) -> void {
  auto* context = resources.d3d_context.context.Get();

  // 设置着色器和资源
  context->IASetInputLayout(resources.basic_shaders.input_layout.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  UINT stride = sizeof(Features::Preview::Types::Vertex);
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, resources.basic_vertex_buffer.GetAddressOf(), &stride, &offset);

  context->VSSetShader(resources.basic_shaders.vertex_shader.Get(), nullptr, 0);
  context->PSSetShader(resources.basic_shaders.pixel_shader.Get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, resources.capture_srv.GetAddressOf());
  context->PSSetSamplers(0, 1, resources.basic_shaders.sampler.GetAddressOf());

  // 设置混合状态
  float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetBlendState(resources.basic_shaders.blend_state.Get(), blendFactor, 0xffffffff);

  // 绘制
  context->Draw(4, 0);
}

auto render_viewport_frame(Core::State::AppState& state,
                           const Features::Preview::Types::RenderingResources& resources) -> void {
  // 更新视口状态
  Features::Preview::Viewport::update_viewport_rect(state);

  // 渲染视口框
  Features::Preview::Viewport::render_viewport_frame(
      state, resources.d3d_context.context.Get(), resources.viewport_shaders.vertex_shader,
      resources.viewport_shaders.pixel_shader, resources.viewport_shaders.input_layout);
}

auto render_frame(Core::State::AppState& state,
                  Microsoft::WRL::ComPtr<ID3D11Texture2D> capture_texture) -> void {
  if (!state.preview->rendering_resources.initialized) {
    return;
  }

  auto& resources = state.preview->rendering_resources;

  // 检查渲染资源是否正忙，如果是则跳过渲染
  if (resources.resources_busy.load(std::memory_order_acquire)) {
    return;
  }

  auto* context = resources.d3d_context.context.Get();

  // 更新捕获SRV（如果需要）
  if (state.preview->create_new_srv && capture_texture) {
    if (auto srv_result = update_capture_srv(state, capture_texture); srv_result) {
      state.preview->create_new_srv = false;
    }
  }

  if (!resources.capture_srv) {
    return;  // 没有可渲染的内容
  }

  // 清除背景
  float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->ClearRenderTargetView(resources.d3d_context.render_target.Get(), clearColor);

  // 设置渲染目标
  ID3D11RenderTargetView* views[] = {resources.d3d_context.render_target.Get()};
  context->OMSetRenderTargets(1, views, nullptr);

  // 设置视口
  D3D11_VIEWPORT viewport = {};
  RECT clientRect;
  GetClientRect(state.preview->hwnd, &clientRect);
  viewport.Width = static_cast<float>(clientRect.right - clientRect.left);
  viewport.Height = static_cast<float>(clientRect.bottom - clientRect.top);
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  // 渲染基本四边形（游戏画面）
  render_basic_quad(resources);

  // 渲染视口框
  render_viewport_frame(state, resources);

  // 显示
  resources.d3d_context.swap_chain->Present(0, 0);
}

}  // namespace Features::Preview::Rendering