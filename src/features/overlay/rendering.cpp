module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <iostream>

module Features.Overlay.Rendering;

import std;
import Core.State;
import Features.Overlay.State;
import Features.Overlay.Types;
import Features.Overlay.Utils;
import Utils.Graphics.D3D;
import Utils.Logger;

namespace Features::Overlay::Rendering {

auto initialize_d3d_rendering(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (overlay_state.rendering.d3d_initialized) {
    return {};
  }

  // 创建D3D上下文
  auto result = ::Utils::Graphics::D3D::create_d3d_context(overlay_state.window.overlay_hwnd,
                                                           overlay_state.window.window_width,
                                                           overlay_state.window.window_height);

  if (!result) {
    auto error_msg = std::format("Failed to initialize D3D rendering: {}", result.error());
    Logger().error(error_msg);
    return std::unexpected(result.error());
  }

  overlay_state.rendering.d3d_context = std::move(result.value());
  overlay_state.rendering.d3d_initialized = true;
  Logger().info("D3D rendering initialized successfully");

  return {};
}

auto initialize_render_states(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (overlay_state.rendering.render_states_initialized) {
    return {};
  }

  // 创建着色器资源
  if (auto result = create_shader_resources(state); !result) {
    auto error_msg = std::format("Failed to create shader resources: {}", result.error());
    Logger().error(error_msg);
    return std::unexpected(result.error());
  }

  overlay_state.rendering.render_states_initialized = true;
  Logger().info("Render states initialized successfully");
  return {};
}

auto resize_swap_chain(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!overlay_state.rendering.d3d_initialized) {
    return std::unexpected("D3D not initialized");
  }

  auto result = ::Utils::Graphics::D3D::resize_swap_chain(overlay_state.rendering.d3d_context,
                                                          overlay_state.window.window_width,
                                                          overlay_state.window.window_height);

  if (!result) {
    return std::unexpected(result.error());
  }

  // 重新创建渲染目标
  return create_render_target(state);
}

auto create_render_target(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  auto result = ::Utils::Graphics::D3D::create_render_target(overlay_state.rendering.d3d_context);
  if (!result) {
    return std::unexpected(result.error());
  }

  return {};
}

auto create_shader_resources(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  auto vertex_code = std::string(Utils::get_vertex_shader_code());
  auto pixel_code = std::string(Utils::get_pixel_shader_code());

  auto result = ::Utils::Graphics::D3D::create_basic_shader_resources(
      overlay_state.rendering.d3d_context.device.Get(), vertex_code, pixel_code);

  if (!result) {
    return std::unexpected(result.error());
  }

  overlay_state.rendering.shader_resources = std::move(result.value());

  // 创建顶点缓冲区
  Types::Vertex vertices[] = {
      {-1.0f, -1.0f, 0.0f, 1.0f},  // 左下
      {-1.0f, 1.0f, 0.0f, 0.0f},   // 左上
      {1.0f, -1.0f, 1.0f, 1.0f},   // 右下
      {1.0f, 1.0f, 1.0f, 0.0f}     // 右上
  };

  auto vertex_buffer_result = ::Utils::Graphics::D3D::create_vertex_buffer(
      overlay_state.rendering.d3d_context.device.Get(), vertices, 4, sizeof(Types::Vertex));

  if (!vertex_buffer_result) {
    return std::unexpected(vertex_buffer_result.error());
  }

  overlay_state.rendering.shader_resources.vertex_buffer = vertex_buffer_result.value();

  return {};
}

auto perform_rendering(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  auto& d3d_context = overlay_state.rendering.d3d_context;
  auto& shader_resources = overlay_state.rendering.shader_resources;

  if (!overlay_state.rendering.d3d_initialized ||
      !overlay_state.rendering.render_states_initialized) {
    return;
  }

  // 等待帧延迟对象
  if (overlay_state.rendering.frame_latency_object) {
    DWORD result = WaitForSingleObjectEx(overlay_state.rendering.frame_latency_object, 1000, TRUE);
    if (result != WAIT_OBJECT_0) {
      // 超时或失败，继续渲染
    }
  }

  // 清理渲染目标
  float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  d3d_context.context->ClearRenderTargetView(d3d_context.render_target.Get(), clear_color);
  d3d_context.context->OMSetRenderTargets(1, d3d_context.render_target.GetAddressOf(), nullptr);

  // 设置视口
  D3D11_VIEWPORT viewport = {};
  viewport.Width = static_cast<float>(overlay_state.window.window_width);
  viewport.Height = static_cast<float>(overlay_state.window.window_height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  d3d_context.context->RSSetViewports(1, &viewport);

  // 设置着色器和资源
  d3d_context.context->VSSetShader(shader_resources.vertex_shader.Get(), nullptr, 0);
  d3d_context.context->PSSetShader(shader_resources.pixel_shader.Get(), nullptr, 0);
  d3d_context.context->IASetInputLayout(shader_resources.input_layout.Get());

  // 设置顶点缓冲区
  UINT stride = sizeof(Types::Vertex);
  UINT offset = 0;
  d3d_context.context->IASetVertexBuffers(0, 1, shader_resources.vertex_buffer.GetAddressOf(),
                                          &stride, &offset);
  d3d_context.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  // 设置纹理和采样器
  if (overlay_state.rendering.shader_resource_view) {
    d3d_context.context->PSSetShaderResources(
        0, 1, overlay_state.rendering.shader_resource_view.GetAddressOf());
  }
  if (shader_resources.sampler) {
    d3d_context.context->PSSetSamplers(0, 1, shader_resources.sampler.GetAddressOf());
  }

  // 设置混合状态
  if (shader_resources.blend_state) {
    float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3d_context.context->OMSetBlendState(shader_resources.blend_state.Get(), blend_factor,
                                         0xffffffff);
  }

  // 绘制
  d3d_context.context->Draw(4, 0);

  // 呈现
  if (auto swap_chain = d3d_context.swap_chain.Get()) {
    swap_chain->Present(0, 0);
  }
}

auto update_texture_resources(Core::State::AppState& state,
                              Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!texture) {
    return std::unexpected("Invalid frame texture");
  }

  // 创建着色器资源视图
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;

  HRESULT hr = overlay_state.rendering.d3d_context.device->CreateShaderResourceView(
      texture.Get(), &srv_desc,
      overlay_state.rendering.shader_resource_view.ReleaseAndGetAddressOf());

  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create shader resource view. HRESULT: 0x{:x}", hr);
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return {};
}

auto render_frame(Core::State::AppState& state,
                  Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void {
  auto& overlay_state = *state.overlay;
  auto& d3d_context = overlay_state.rendering.d3d_context;
  auto& shader_resources = overlay_state.rendering.shader_resources;

  if (!overlay_state.rendering.d3d_initialized ||
      !overlay_state.rendering.render_states_initialized) {
    return;
  }

  // 等待帧延迟对象
  if (overlay_state.rendering.frame_latency_object) {
    DWORD result = WaitForSingleObjectEx(overlay_state.rendering.frame_latency_object, 1000, TRUE);
    if (result != WAIT_OBJECT_0) {
      // 超时或失败，继续渲染
    }
  }

  // 更新纹理资源
  if (overlay_state.rendering.create_new_srv) {
    if (auto result = update_texture_resources(state, frame_texture); result) {
      overlay_state.rendering.create_new_srv = false;
    }
  }

  // 清理渲染目标
  float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  d3d_context.context->ClearRenderTargetView(d3d_context.render_target.Get(), clear_color);
  d3d_context.context->OMSetRenderTargets(1, d3d_context.render_target.GetAddressOf(), nullptr);

  // 设置视口
  D3D11_VIEWPORT viewport = {};
  viewport.Width = static_cast<float>(overlay_state.window.window_width);
  viewport.Height = static_cast<float>(overlay_state.window.window_height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  d3d_context.context->RSSetViewports(1, &viewport);

  // 设置着色器和资源
  d3d_context.context->VSSetShader(shader_resources.vertex_shader.Get(), nullptr, 0);
  d3d_context.context->PSSetShader(shader_resources.pixel_shader.Get(), nullptr, 0);
  d3d_context.context->IASetInputLayout(shader_resources.input_layout.Get());

  // 设置顶点缓冲区
  UINT stride = sizeof(Types::Vertex);
  UINT offset = 0;
  d3d_context.context->IASetVertexBuffers(0, 1, shader_resources.vertex_buffer.GetAddressOf(),
                                          &stride, &offset);
  d3d_context.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  // 设置纹理和采样器
  if (overlay_state.rendering.shader_resource_view) {
    d3d_context.context->PSSetShaderResources(
        0, 1, overlay_state.rendering.shader_resource_view.GetAddressOf());
  }
  if (shader_resources.sampler) {
    d3d_context.context->PSSetSamplers(0, 1, shader_resources.sampler.GetAddressOf());
  }

  // 设置混合状态
  if (shader_resources.blend_state) {
    float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3d_context.context->OMSetBlendState(shader_resources.blend_state.Get(), blend_factor,
                                         0xffffffff);
  }

  // 绘制
  d3d_context.context->Draw(4, 0);

  // 呈现
  if (auto swap_chain = d3d_context.swap_chain.Get()) {
    swap_chain->Present(0, 0);
  }
}

auto cleanup_rendering_resources(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  ::Utils::Graphics::D3D::cleanup_shader_resources(overlay_state.rendering.shader_resources);
  ::Utils::Graphics::D3D::cleanup_d3d_context(overlay_state.rendering.d3d_context);

  overlay_state.rendering.frame_texture.Reset();
  overlay_state.rendering.shader_resource_view.Reset();

  if (overlay_state.rendering.frame_latency_object) {
    CloseHandle(overlay_state.rendering.frame_latency_object);
    overlay_state.rendering.frame_latency_object = nullptr;
  }

  overlay_state.rendering.d3d_initialized = false;
  overlay_state.rendering.render_states_initialized = false;
}

auto has_new_frame(const Core::State::AppState& state) -> bool {
  return state.overlay->rendering.has_new_frame;
}

auto set_new_frame_flag(Core::State::AppState& state, bool has_frame) -> void {
  state.overlay->rendering.has_new_frame = has_frame;
}

}  // namespace Features::Overlay::Rendering
