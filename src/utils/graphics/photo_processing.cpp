module;

module Utils.Graphics.PhotoProcessing;

import std;
import Utils.Graphics.D3D;
import Utils.Logger;
import <d3d11.h>;
import <wil/com.h>;

namespace Utils::Graphics::PhotoProcessing {

// HLSL constant buffer，与 shader 中的 AverageConstants 对齐（16 字节对齐）
struct AverageConstants {
  float current_weight = 1.0f;
  float padding0 = 0.0f;
  float padding1 = 0.0f;
  float padding2 = 0.0f;
};

// 全屏三角形顶点着色器，通过 SV_VertexID 生成覆盖整个屏幕的三角形，无需顶点缓冲
const std::string kFullscreenVertexShader = R"(
struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(uint vertexId : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };
    float2 uvs[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    VS_OUTPUT output;
    output.pos = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = uvs[vertexId];
    return output;
}
)";

// 逐像素加权混合：result = lerp(历史均值, 当前帧, 1/N)，N 为当前帧序号
// 这样每帧权重相等，最终得到 N 帧的算术平均
const std::string kAveragePixelShader = R"(
Texture2D<float4> gCurrent : register(t0);
Texture2D<float4> gPreviousAverage : register(t1);
SamplerState gSampler : register(s0);

cbuffer AverageConstants : register(b0) {
    float gCurrentWeight;
    float3 gPadding;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target {
    float4 current = gCurrent.Sample(gSampler, uv);
    float4 previous = gPreviousAverage.Sample(gSampler, uv);
    return lerp(previous, current, gCurrentWeight);
}
)";

auto hresult_error(std::string_view message, HRESULT hr) -> std::string {
  return std::format("{} HRESULT=0x{:08X}", message, static_cast<unsigned>(hr));
}

auto hresult_failed(HRESULT hr) -> bool { return hr < 0; }

// 从纹理反向获取所属 D3D 设备和即时上下文，后续操作都需要这两者
auto get_texture_device(ID3D11Texture2D* texture)
    -> std::expected<std::pair<wil::com_ptr<ID3D11Device>, wil::com_ptr<ID3D11DeviceContext>>,
                     std::string> {
  if (!texture) {
    return std::unexpected("Texture is null");
  }

  wil::com_ptr<ID3D11Device> device;
  texture->GetDevice(device.put());
  if (!device) {
    return std::unexpected("Texture device is null");
  }

  wil::com_ptr<ID3D11DeviceContext> context;
  device->GetImmediateContext(context.put());
  if (!context) {
    return std::unexpected("Immediate context is null");
  }

  return std::pair{device, context};
}

// 创建可用于着色器读写（SRV+RTV）的中间纹理，规格复制自源帧
auto create_processing_texture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source_desc)
    -> std::expected<wil::com_ptr<ID3D11Texture2D>, std::string> {
  if (!device) {
    return std::unexpected("D3D device is null");
  }

  D3D11_TEXTURE2D_DESC desc = source_desc;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  wil::com_ptr<ID3D11Texture2D> texture;
  HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.put());
  if (hresult_failed(hr) || !texture) {
    return std::unexpected(hresult_error("Failed to create processing texture", hr));
  }

  return texture;
}

auto create_texture_srv(ID3D11Device* device, ID3D11Texture2D* texture)
    -> std::expected<wil::com_ptr<ID3D11ShaderResourceView>, std::string> {
  if (!device || !texture) {
    return std::unexpected("Invalid resources for SRV creation");
  }

  D3D11_TEXTURE2D_DESC desc{};
  texture->GetDesc(&desc);

  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Format = desc.Format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MostDetailedMip = 0;
  srv_desc.Texture2D.MipLevels = 1;

  wil::com_ptr<ID3D11ShaderResourceView> srv;
  HRESULT hr = device->CreateShaderResourceView(texture, &srv_desc, srv.put());
  if (hresult_failed(hr) || !srv) {
    return std::unexpected(hresult_error("Failed to create shader resource view", hr));
  }

  return srv;
}

auto create_texture_rtv(ID3D11Device* device, ID3D11Texture2D* texture)
    -> std::expected<wil::com_ptr<ID3D11RenderTargetView>, std::string> {
  if (!device || !texture) {
    return std::unexpected("Invalid resources for RTV creation");
  }

  D3D11_TEXTURE2D_DESC desc{};
  texture->GetDesc(&desc);

  D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
  rtv_desc.Format = desc.Format;
  rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtv_desc.Texture2D.MipSlice = 0;

  wil::com_ptr<ID3D11RenderTargetView> rtv;
  HRESULT hr = device->CreateRenderTargetView(texture, &rtv_desc, rtv.put());
  if (hresult_failed(hr) || !rtv) {
    return std::unexpected(hresult_error("Failed to create render target view", hr));
  }

  return rtv;
}

template <typename Constants>
auto create_constant_buffer(ID3D11Device* device, const Constants& constants)
    -> std::expected<wil::com_ptr<ID3D11Buffer>, std::string> {
  static_assert(sizeof(Constants) % 16 == 0);

  D3D11_BUFFER_DESC desc{};
  desc.ByteWidth = sizeof(Constants);
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

  D3D11_SUBRESOURCE_DATA initial_data{};
  initial_data.pSysMem = &constants;

  wil::com_ptr<ID3D11Buffer> buffer;
  HRESULT hr = device->CreateBuffer(&desc, &initial_data, buffer.put());
  if (hresult_failed(hr) || !buffer) {
    return std::unexpected(hresult_error("Failed to create constant buffer", hr));
  }

  return buffer;
}

auto create_vertex_shader(ID3D11Device* device)
    -> std::expected<wil::com_ptr<ID3D11VertexShader>, std::string> {
  auto blob_result =
      Utils::Graphics::D3D::compile_shader(kFullscreenVertexShader, "main", "vs_4_0");
  if (!blob_result) {
    return std::unexpected(blob_result.error());
  }

  wil::com_ptr<ID3D11VertexShader> shader;
  HRESULT hr = device->CreateVertexShader((*blob_result)->GetBufferPointer(),
                                          (*blob_result)->GetBufferSize(), nullptr, shader.put());
  if (hresult_failed(hr) || !shader) {
    return std::unexpected(hresult_error("Failed to create vertex shader", hr));
  }

  return shader;
}

auto create_pixel_shader(ID3D11Device* device, const std::string& source, std::string_view label)
    -> std::expected<wil::com_ptr<ID3D11PixelShader>, std::string> {
  auto blob_result = Utils::Graphics::D3D::compile_shader(source, "main", "ps_4_0");
  if (!blob_result) {
    return std::unexpected(std::format("{} shader compile failed: {}", label, blob_result.error()));
  }

  wil::com_ptr<ID3D11PixelShader> shader;
  HRESULT hr = device->CreatePixelShader((*blob_result)->GetBufferPointer(),
                                         (*blob_result)->GetBufferSize(), nullptr, shader.put());
  if (hresult_failed(hr) || !shader) {
    return std::unexpected(
        hresult_error(std::format("Failed to create {} pixel shader", label), hr));
  }

  return shader;
}

auto create_sampler(ID3D11Device* device)
    -> std::expected<wil::com_ptr<ID3D11SamplerState>, std::string> {
  return Utils::Graphics::D3D::create_linear_sampler(device);
}

// 渲染完成后解绑所有管线资源，避免意外引用导致 GPU 资源泄漏
auto clear_pipeline_bindings(ID3D11DeviceContext* context) -> void {
  ID3D11ShaderResourceView* null_srvs[2] = {nullptr, nullptr};
  ID3D11RenderTargetView* null_rtvs[1] = {nullptr};
  ID3D11Buffer* null_cbs[1] = {nullptr};
  ID3D11SamplerState* null_samplers[1] = {nullptr};

  context->PSSetShaderResources(0, 2, null_srvs);
  context->PSSetSamplers(0, 1, null_samplers);
  context->PSSetConstantBuffers(0, 1, null_cbs);
  context->OMSetRenderTargets(1, null_rtvs, nullptr);
  context->VSSetShader(nullptr, nullptr, 0);
  context->PSSetShader(nullptr, nullptr, 0);
}

// 用全屏三角形将两张纹理混合到一张 RTV 上，是帧累积的核心渲染步骤
auto render_fullscreen(ID3D11DeviceContext* context, ID3D11VertexShader* vertex_shader,
                       ID3D11PixelShader* pixel_shader, ID3D11RenderTargetView* rtv,
                       ID3D11ShaderResourceView* srv0, ID3D11ShaderResourceView* srv1,
                       ID3D11SamplerState* sampler, ID3D11Buffer* constant_buffer, UINT width,
                       UINT height) -> void {
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(width);
  viewport.Height = static_cast<float>(height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  ID3D11RenderTargetView* rtvs[] = {rtv};
  context->OMSetRenderTargets(1, rtvs, nullptr);
  context->IASetInputLayout(nullptr);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vertex_shader, nullptr, 0);
  context->PSSetShader(pixel_shader, nullptr, 0);

  ID3D11ShaderResourceView* srvs[] = {srv0, srv1};
  context->PSSetShaderResources(0, 2, srvs);
  context->PSSetSamplers(0, 1, &sampler);
  context->PSSetConstantBuffers(0, 1, &constant_buffer);
  context->Draw(3, 0);
  clear_pipeline_bindings(context);
}

// 一次性创建双缓冲纹理、视图、着色器、采样器等全部 GPU 资源
auto create_average_accumulator_resources(ID3D11Device* device,
                                          const D3D11_TEXTURE2D_DESC& source_desc)
    -> std::expected<AverageAccumulator, std::string> {
  auto average_result = create_processing_texture(device, source_desc);
  auto scratch_result = create_processing_texture(device, source_desc);
  if (!average_result) return std::unexpected(average_result.error());
  if (!scratch_result) return std::unexpected(scratch_result.error());

  auto average_srv_result = create_texture_srv(device, average_result->get());
  auto scratch_srv_result = create_texture_srv(device, scratch_result->get());
  auto average_rtv_result = create_texture_rtv(device, average_result->get());
  auto scratch_rtv_result = create_texture_rtv(device, scratch_result->get());
  auto vs_result = create_vertex_shader(device);
  auto ps_result = create_pixel_shader(device, kAveragePixelShader, "average");
  auto sampler_result = create_sampler(device);
  auto constants_result = create_constant_buffer(device, AverageConstants{});

  if (!average_srv_result) return std::unexpected(average_srv_result.error());
  if (!scratch_srv_result) return std::unexpected(scratch_srv_result.error());
  if (!average_rtv_result) return std::unexpected(average_rtv_result.error());
  if (!scratch_rtv_result) return std::unexpected(scratch_rtv_result.error());
  if (!vs_result) return std::unexpected(vs_result.error());
  if (!ps_result) return std::unexpected(ps_result.error());
  if (!sampler_result) return std::unexpected(sampler_result.error());
  if (!constants_result) return std::unexpected(constants_result.error());

  return AverageAccumulator{
      .current_average = std::move(average_result.value()),
      .scratch = std::move(scratch_result.value()),
      .current_average_srv = std::move(average_srv_result.value()),
      .scratch_srv = std::move(scratch_srv_result.value()),
      .current_average_rtv = std::move(average_rtv_result.value()),
      .scratch_rtv = std::move(scratch_rtv_result.value()),
      .fullscreen_vertex_shader = std::move(vs_result.value()),
      .average_pixel_shader = std::move(ps_result.value()),
      .sampler = std::move(sampler_result.value()),
      .constants_buffer = std::move(constants_result.value()),
      .width = source_desc.Width,
      .height = source_desc.Height,
      .format = source_desc.Format,
  };
}

auto has_average_accumulator_resources(const AverageAccumulator& accumulator) -> bool {
  return accumulator.current_average && accumulator.scratch && accumulator.current_average_srv &&
         accumulator.scratch_srv && accumulator.current_average_rtv && accumulator.scratch_rtv &&
         accumulator.fullscreen_vertex_shader && accumulator.average_pixel_shader &&
         accumulator.sampler && accumulator.constants_buffer && accumulator.width > 0 &&
         accumulator.height > 0 && accumulator.format != DXGI_FORMAT_UNKNOWN &&
         accumulator.frame_count > 0;
}

// 用第一帧初始化累积器：拷贝到 average 纹理作为初始均值，frame_count=1
auto initialize_average_accumulator(ID3D11Texture2D* first_frame)
    -> std::expected<AverageAccumulator, std::string> {
  if (!first_frame) {
    return std::unexpected("First frame is null");
  }

  auto device_context_result = get_texture_device(first_frame);
  if (!device_context_result) {
    return std::unexpected(device_context_result.error());
  }
  auto [device, context] = std::move(device_context_result.value());

  D3D11_TEXTURE2D_DESC source_desc{};
  first_frame->GetDesc(&source_desc);

  auto accumulator_result = create_average_accumulator_resources(device.get(), source_desc);
  if (!accumulator_result) {
    return std::unexpected(accumulator_result.error());
  }

  auto accumulator = std::move(accumulator_result.value());
  context->CopyResource(accumulator.current_average.get(), first_frame);
  accumulator.frame_count = 1;

  return std::move(accumulator);
}

// 将新帧混合到当前均值：weight=1/N 保证每帧等权，然后交换双缓冲
auto accumulate_average_frame(AverageAccumulator& accumulator, ID3D11Texture2D* frame)
    -> std::expected<void, std::string> {
  if (!frame) {
    return std::unexpected("Frame is null");
  }
  if (!has_average_accumulator_resources(accumulator)) {
    return std::unexpected("Average accumulator resources are not initialized");
  }

  auto device_context_result = get_texture_device(frame);
  if (!device_context_result) {
    return std::unexpected(device_context_result.error());
  }
  auto [device, context] = std::move(device_context_result.value());

  D3D11_TEXTURE2D_DESC frame_desc{};
  frame->GetDesc(&frame_desc);
  if (frame_desc.Width != accumulator.width || frame_desc.Height != accumulator.height ||
      frame_desc.Format != accumulator.format) {
    return std::unexpected("Average accumulator frame size or format changed");
  }

  wil::com_ptr<ID3D11Device> accumulator_device;
  accumulator.current_average->GetDevice(accumulator_device.put());
  if (accumulator_device.get() != device.get()) {
    return std::unexpected("Average accumulator D3D device changed");
  }

  const auto next_frame_count = accumulator.frame_count + 1;
  // weight = 1/N：第 N 帧混合时，历史均值占 (N-1)/N，新帧占 1/N
  const float current_weight = 1.0f / static_cast<float>(next_frame_count);

  auto current_srv_result = create_texture_srv(device.get(), frame);
  if (!current_srv_result) return std::unexpected(current_srv_result.error());

  const AverageConstants constants{.current_weight = current_weight};
  context->UpdateSubresource(accumulator.constants_buffer.get(), 0, nullptr, &constants, 0, 0);

  render_fullscreen(context.get(), accumulator.fullscreen_vertex_shader.get(),
                    accumulator.average_pixel_shader.get(), accumulator.scratch_rtv.get(),
                    current_srv_result->get(), accumulator.current_average_srv.get(),
                    accumulator.sampler.get(), accumulator.constants_buffer.get(), frame_desc.Width,
                    frame_desc.Height);

  // 双缓冲交换：scratch 变成新的 average，旧 average 下次变为 scratch
  std::swap(accumulator.current_average, accumulator.scratch);
  std::swap(accumulator.current_average_srv, accumulator.scratch_srv);
  std::swap(accumulator.current_average_rtv, accumulator.scratch_rtv);
  accumulator.frame_count = next_frame_count;
  return {};
}

}  // namespace Utils::Graphics::PhotoProcessing
