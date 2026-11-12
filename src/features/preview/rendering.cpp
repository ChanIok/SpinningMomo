#include "features/preview/rendering.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/winerror.hpp"

#include "core/state/app_state.hpp"
#include "features/preview/shaders.hpp"
#include "features/preview/state.hpp"
#include "features/preview/types.hpp"
#include "features/preview/viewport.hpp"
#include "features/settings/state.hpp"
#include "utils/graphics/d3d.hpp"
#include "utils/logger/logger.hpp"

namespace features::preview::rendering {

auto create_basic_vertex_buffer(ID3D11Device* device)
    -> std::expected<wil::com_ptr<ID3D11Buffer>, std::string> {
  // 创建全屏四边形的顶点数据
  features::preview::Vertex vertices[] = {
      {-1.0f, 1.0f, 0.0f, 0.0f},   // 左上
      {1.0f, 1.0f, 1.0f, 0.0f},    // 右上
      {-1.0f, -1.0f, 0.0f, 1.0f},  // 左下
      {1.0f, -1.0f, 1.0f, 1.0f}    // 右下
  };

  auto buffer_result = utils::graphics::d3d::create_vertex_buffer(
      device, vertices, 4, sizeof(features::preview::Vertex));

  if (!buffer_result) {
    return std::unexpected("Failed to create vertex buffer");
  }

  return buffer_result.value();
}

auto initialize_rendering(core::AppState& state, HWND hwnd, int width, int height)
    -> std::expected<void, std::string> {
  auto& resources = state.preview->rendering_resources;

  resources.resize_error.reset();
  // 创建D3D上下文
  auto d3d_result =
      utils::graphics::d3d::create_d3d_context(hwnd, width, height, state.preview->enable_hdr,
                                               state.settings->raw.app.prefer_high_performance_gpu);
  if (!d3d_result) {
    Logger().error("Failed to create D3D context for preview rendering: {}", d3d_result.error());
    return std::unexpected(d3d_result.error().find("device") != std::string::npos
                               ? "Failed to initialize D3D device"
                               : "Failed to create D3D resources");
  }
  resources.d3d_context = std::move(d3d_result.value());

  // 创建基本渲染着色器
  auto basic_shader_result = utils::graphics::d3d::create_basic_shader_resources(
      resources.d3d_context.device.get(), features::preview::shaders::BASIC_VERTEX_SHADER,
      features::preview::shaders::BASIC_PIXEL_SHADER);

  if (!basic_shader_result) {
    Logger().error("Failed to create basic shader resources");
    return std::unexpected("Failed to compile basic shaders");
  }
  resources.basic_shaders = std::move(basic_shader_result.value());

  // 创建视口框着色器
  auto viewport_shader_result = utils::graphics::d3d::create_viewport_shader_resources(
      resources.d3d_context.device.get(), features::preview::shaders::VIEWPORT_VERTEX_SHADER,
      features::preview::shaders::VIEWPORT_PIXEL_SHADER);

  if (!viewport_shader_result) {
    Logger().error("Failed to create viewport shader resources");
    return std::unexpected("Failed to compile viewport shaders");
  }
  resources.viewport_shaders = std::move(viewport_shader_result.value());

  // 创建基本顶点缓冲区（全屏四边形）
  auto vertex_buffer_result = create_basic_vertex_buffer(resources.d3d_context.device.get());
  if (!vertex_buffer_result) {
    return std::unexpected(vertex_buffer_result.error());
  }
  resources.basic_vertex_buffer = std::move(vertex_buffer_result.value());

  resources.initialized.store(true, std::memory_order_release);

  Logger().info("Preview rendering system initialized successfully");
  return {};
}

auto cleanup_rendering(core::AppState& state) -> void {
  auto& resources = state.preview->rendering_resources;

  resources.initialized.store(false, std::memory_order_release);
  // 清理过程中跳过到达的捕获帧。
  resources.resources_busy.store(true, std::memory_order_release);
  release_capture_surface(state);

  // 清理着色器资源
  utils::graphics::d3d::cleanup_shader_resources(resources.basic_shaders);
  utils::graphics::d3d::cleanup_shader_resources(resources.viewport_shaders);

  // 清理D3D上下文
  utils::graphics::d3d::cleanup_d3d_context(resources.d3d_context);

  // 重置资源
  resources.basic_vertex_buffer.reset();
  resources.viewport_vertex_buffer.reset();
  resources.resources_busy.store(false, std::memory_order_release);

  Logger().info("Preview rendering resources cleaned up");
}

auto release_capture_surface(core::AppState& state) -> void {
  auto& resources = state.preview->rendering_resources;
  if (resources.d3d_context.context) {
    ID3D11ShaderResourceView* null_srv = nullptr;
    resources.d3d_context.context->PSSetShaderResources(0, 1, &null_srv);
    resources.d3d_context.context->Flush();
  }
  resources.capture_srv.reset();
}

auto resize_rendering(core::AppState& state, int width, int height)
    -> std::expected<void, std::string> {
  if (!state.preview->rendering_resources.initialized.load(std::memory_order_acquire)) {
    return std::unexpected("D3D not initialized");
  }

  auto& resources = state.preview->rendering_resources;
  resources.resize_error.reset();
  // 标记交换链调整状态；回调若观察到此标记就跳过当前帧。
  resources.resources_busy.store(true, std::memory_order_release);

  // 调整交换链大小
  auto resize_result =
      utils::graphics::d3d::resize_swap_chain(resources.d3d_context, width, height);
  resources.resources_busy.store(false, std::memory_order_release);

  if (!resize_result) {
    resources.resize_error = resize_result.error();
    return std::unexpected(resize_result.error());
  }

  Logger().debug("Preview rendering resized to {}x{}", width, height);
  return {};
}

auto create_capture_srv(ID3D11Device* device, ID3D11Texture2D* texture)
    -> std::expected<wil::com_ptr<ID3D11ShaderResourceView>, std::string> {
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
  wil::com_ptr<ID3D11ShaderResourceView> srv;
  HRESULT hr = device->CreateShaderResourceView(texture, &srvDesc, srv.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create preview SRV, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }

  return srv;
}

auto matches_capture_texture(ID3D11ShaderResourceView* srv, ID3D11Texture2D* texture)
    -> std::expected<bool, std::string> {
  if (!srv) {
    return false;
  }

  wil::com_ptr<ID3D11Resource> resource;
  srv->GetResource(resource.put());
  // 不同接口指针可能属于同一 COM 对象，统一比较 IUnknown 身份。
  wil::com_ptr<IUnknown> cached_identity;
  auto hr = resource->QueryInterface(IID_PPV_ARGS(cached_identity.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to query cached preview texture identity, HRESULT: 0x{:08X}",
                    static_cast<unsigned int>(hr)));
  }
  wil::com_ptr<IUnknown> current_identity;
  hr = texture->QueryInterface(IID_PPV_ARGS(current_identity.put()));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to query preview texture identity, HRESULT: 0x{:08X}",
                    static_cast<unsigned int>(hr)));
  }
  return cached_identity.get() == current_identity.get();
}

auto render_basic_quad(const features::preview::RenderingResources& resources,
                       ID3D11ShaderResourceView* srv) -> void {
  auto* context = resources.d3d_context.context.get();

  // 设置着色器和资源
  context->IASetInputLayout(resources.basic_shaders.input_layout.get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  UINT stride = sizeof(features::preview::Vertex);
  UINT offset = 0;
  ID3D11Buffer* vertex_buffer = resources.basic_vertex_buffer.get();
  context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

  context->VSSetShader(resources.basic_shaders.vertex_shader.get(), nullptr, 0);
  context->PSSetShader(resources.basic_shaders.pixel_shader.get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &srv);
  ID3D11SamplerState* sampler = resources.basic_shaders.sampler.get();
  context->PSSetSamplers(0, 1, &sampler);

  // 设置混合状态
  float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetBlendState(resources.basic_shaders.blend_state.get(), blendFactor, 0xffffffff);

  // 绘制
  context->Draw(4, 0);
}

auto render_viewport_frame(core::AppState& state,
                           const features::preview::RenderingResources& resources) -> void {
  // 更新视口状态
  features::preview::viewport::update_viewport_rect(state);

  // 渲染视口框
  features::preview::viewport::render_viewport_frame(
      state, resources.d3d_context.context.get(), resources.viewport_shaders.vertex_shader,
      resources.viewport_shaders.pixel_shader, resources.viewport_shaders.input_layout);
}

auto render_frame(core::AppState& state, ID3D11Texture2D* capture_texture)
    -> std::expected<void, std::string> {
  auto& resources = state.preview->rendering_resources;
  if (!resources.initialized.load(std::memory_order_acquire) || !capture_texture) {
    return std::unexpected("Invalid preview rendering resources or texture");
  }
  // 交换链尺寸调整时丢弃当前帧；下一次捕获回调会继续刷新预览。
  if (resources.resources_busy.load(std::memory_order_acquire)) {
    return {};
  }
  if (!resources.d3d_context.render_target) {
    return std::unexpected("Preview render target is unavailable");
  }

  auto* context = resources.d3d_context.context.get();
  auto matches = matches_capture_texture(resources.capture_srv.get(), capture_texture);
  if (!matches) {
    return std::unexpected(matches.error());
  }
  if (!*matches) {
    // 帧池重建后纹理对象会改变，此时才重建缓存的 SRV。
    release_capture_surface(state);
    auto srv = create_capture_srv(resources.d3d_context.device.get(), capture_texture);
    if (!srv) {
      return std::unexpected(srv.error());
    }
    resources.capture_srv = std::move(*srv);
  }
  // context 也会持有绑定的 SRV，必须在归还捕获帧前解除上下文绑定。
  auto unbind_capture = wil::scope_exit([&] {
    ID3D11ShaderResourceView* null_srv = nullptr;
    context->PSSetShaderResources(0, 1, &null_srv);
  });

  // 清除背景
  float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->ClearRenderTargetView(resources.d3d_context.render_target.get(), clearColor);

  // 设置渲染目标
  ID3D11RenderTargetView* views[] = {resources.d3d_context.render_target.get()};
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
  render_basic_quad(resources, resources.capture_srv.get());

  // 渲染视口框
  render_viewport_frame(state, resources);

  // 显示
  const auto hr = resources.d3d_context.swap_chain->Present(0, 0);
  if (FAILED(hr)) {
    return std::unexpected(std::format(
        "Preview Present failed, HRESULT: 0x{:08X}, device status: 0x{:08X}",
        static_cast<unsigned int>(hr),
        static_cast<unsigned int>(resources.d3d_context.device->GetDeviceRemovedReason())));
  }
  return {};
}

}  // namespace features::preview::rendering
