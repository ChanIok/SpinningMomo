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
import Features.Overlay.Shaders;
import Utils.Graphics.D3D;
import Utils.Logger;

namespace Features::Overlay::Rendering {

auto create_shader_resources(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  auto result = ::Utils::Graphics::D3D::create_basic_shader_resources(
      overlay_state.rendering.d3d_context.device.Get(),
      Features::Overlay::Shaders::BASIC_VERTEX_SHADER,
      Features::Overlay::Shaders::BASIC_PIXEL_SHADER);

  if (!result) {
    return std::unexpected(result.error());
  }

  overlay_state.rendering.shader_resources = std::move(result.value());

  // 创建顶点缓冲区，初始使用全屏顶点
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

auto initialize_rendering(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  // 创建D3D上下文
  auto d3d_result = ::Utils::Graphics::D3D::create_d3d_context(overlay_state.window.overlay_hwnd,
                                                               overlay_state.window.window_width,
                                                               overlay_state.window.window_height);

  if (!d3d_result) {
    auto error_msg = std::format("Failed to initialize D3D rendering: {}", d3d_result.error());
    Logger().error(error_msg);
    return std::unexpected(d3d_result.error());
  }

  overlay_state.rendering.d3d_context = std::move(d3d_result.value());
  Logger().info("D3D rendering initialized successfully");

  // 创建着色器资源
  if (auto result = create_shader_resources(state); !result) {
    auto error_msg = std::format("Failed to create shader resources: {}", result.error());
    Logger().error(error_msg);

    // 清理已分配的D3D资源
    ::Utils::Graphics::D3D::cleanup_d3d_context(overlay_state.rendering.d3d_context);
    overlay_state.rendering.d3d_initialized = false;

    return std::unexpected(result.error());
  }

  overlay_state.rendering.d3d_initialized = true;
  Logger().info("Render states initialized successfully");

  return {};
}

auto resize_rendering(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.overlay->rendering.d3d_initialized) {
    return std::unexpected("D3D not initialized");
  }

  auto& overlay_state = *state.overlay;
  auto& rendering_state = overlay_state.rendering;

  rendering_state.resources_busy.store(true, std::memory_order_release);

  // 调整交换链大小
  auto result = ::Utils::Graphics::D3D::resize_swap_chain(rendering_state.d3d_context,
                                                          overlay_state.window.window_width,
                                                          overlay_state.window.window_height);

  rendering_state.resources_busy.store(false, std::memory_order_release);

  if (!result) {
    Logger().error("Failed to resize swap chain for overlay: {}", result.error());
    return std::unexpected("Failed to resize swap chain");
  }

  Logger().debug("Overlay rendering resized to {}x{}", overlay_state.window.window_width,
                 overlay_state.window.window_height);

  return {};
}

auto update_capture_srv(Core::State::AppState& state,
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> std::expected<void, std::string> {
  auto& overlay_state = *state.overlay;

  if (!overlay_state.rendering.d3d_initialized || !texture) {
    return std::unexpected("Invalid rendering resources or texture");
  }

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
  HRESULT hr = overlay_state.rendering.d3d_context.device->CreateShaderResourceView(
      texture.Get(), &srvDesc, overlay_state.rendering.capture_srv.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    auto error_msg = std::format("Failed to create shader resource view, HRESULT: 0x{:08X}",
                                 static_cast<unsigned int>(hr));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }

  return {};
}

auto update_vertex_buffer_for_letterbox(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;
  auto& d3d_context = overlay_state.rendering.d3d_context;
  auto& shader_resources = overlay_state.rendering.shader_resources;

  // 计算顶点坐标
  float left = -1.0f, right = 1.0f, top = 1.0f, bottom = -1.0f;

  if (overlay_state.window.use_letterbox_mode) {
    // 使用工具函数计算黑边区域
    auto [content_left, content_top, content_width, content_height] = 
        Utils::calculate_letterbox_area(
            overlay_state.window.screen_width,
            overlay_state.window.screen_height,
            overlay_state.window.cached_game_width,
            overlay_state.window.cached_game_height);
    
    // 将像素坐标转换为标准化设备坐标(-1到1)
    // 注意：Direct3D的Y轴是向上的，而窗口坐标Y轴是向下的
    left = (content_left * 2.0f / overlay_state.window.screen_width) - 1.0f;
    right = ((content_left + content_width) * 2.0f / overlay_state.window.screen_width) - 1.0f;
    top = 1.0f - (content_top * 2.0f / overlay_state.window.screen_height);
    bottom = 1.0f - ((content_top + content_height) * 2.0f / overlay_state.window.screen_height);
  }

  // 更新顶点数据
  Types::Vertex vertices[] = {
      {left, bottom, 0.0f, 1.0f},  // 左下
      {left, top, 0.0f, 0.0f},     // 左上
      {right, bottom, 1.0f, 1.0f}, // 右下
      {right, top, 1.0f, 0.0f}     // 右上
  };
  
  // 更新顶点缓冲区
  d3d_context.context->UpdateSubresource(shader_resources.vertex_buffer.Get(), 0, nullptr, vertices, 0, 0);
}

auto render_frame(Core::State::AppState& state,
                  Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void {
  auto& overlay_state = *state.overlay;
  auto& d3d_context = overlay_state.rendering.d3d_context;
  auto& shader_resources = overlay_state.rendering.shader_resources;
  auto& rendering_state = overlay_state.rendering;

  if (!rendering_state.d3d_initialized) {
    return;
  }

  // 检查渲染资源是否正忙，如果是则跳过渲染
  if (rendering_state.resources_busy.load(std::memory_order_acquire)) {
    return;
  }

  // 等待帧延迟对象
  if (rendering_state.frame_latency_object) {
    DWORD result = WaitForSingleObjectEx(rendering_state.frame_latency_object, 1000, TRUE);
    if (result != WAIT_OBJECT_0) {
      // 超时或失败，继续渲染
    }
  }

  // 更新纹理资源
  if (rendering_state.create_new_srv) {
    if (auto result = update_capture_srv(state, frame_texture); result) {
      rendering_state.create_new_srv = false;
    }
  }

  // 清理渲染目标为黑色（作为黑边的背景色）
  float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  d3d_context.context->ClearRenderTargetView(d3d_context.render_target.Get(), clear_color);
  d3d_context.context->OMSetRenderTargets(1, d3d_context.render_target.GetAddressOf(), nullptr);

  // 根据letterbox模式更新顶点缓冲区
  update_vertex_buffer_for_letterbox(state);

  // 设置视口和渲染参数
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
  if (rendering_state.capture_srv) {
    d3d_context.context->PSSetShaderResources(0, 1, rendering_state.capture_srv.GetAddressOf());
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

auto cleanup_rendering(Core::State::AppState& state) -> void {
  auto& overlay_state = *state.overlay;

  ::Utils::Graphics::D3D::cleanup_shader_resources(overlay_state.rendering.shader_resources);
  ::Utils::Graphics::D3D::cleanup_d3d_context(overlay_state.rendering.d3d_context);

  overlay_state.rendering.frame_texture.Reset();
  overlay_state.rendering.capture_srv.Reset();

  if (overlay_state.rendering.frame_latency_object) {
    CloseHandle(overlay_state.rendering.frame_latency_object);
    overlay_state.rendering.frame_latency_object = nullptr;
  }

  overlay_state.rendering.d3d_initialized = false;
}

}  // namespace Features::Overlay::Rendering
