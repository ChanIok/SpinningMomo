module;

module Utils.Media.HdrConvert;

import std;
import Utils.Graphics.D3D;
import Utils.Logger;
import <d3d11.h>;
import <d3d11_4.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Media::HdrConvert {

const std::string k_fullscreen_vertex_shader = R"(
struct VSOut {
    float4 position : SV_Position;
};

VSOut main(uint vertex_id : SV_VertexID) {
    float id_high = float(vertex_id >> 1);
    float id_low = float(vertex_id & 1u);

    VSOut output;
    output.position = float4(id_high * 4.0f - 1.0f, id_low * 4.0f - 1.0f, 0.0f, 1.0f);
    return output;
}
)";

const std::string k_p010_luma_pixel_shader = R"(
Texture2D<float4> gSource : register(t0);

static const float kSourceToPqScale = 80.0f / 10000.0f;
static const float kKr = 0.2627f;
static const float kKb = 0.0593f;
static const float kKg = 1.0f - kKr - kKb;

float3 rec709_to_rec2020(float3 value) {
    float r = dot(value, float3(0.62740389593469903f, 0.32928303837788370f, 0.043313065687417225f));
    float g = dot(value, float3(0.069097289358232075f, 0.91954039507545871f, 0.011362315566309178f));
    float b = dot(value, float3(0.016391438875150280f, 0.088013307877225749f, 0.89559525324762401f));
    return float3(r, g, b);
}

float linear_to_st2084_channel(float value) {
    float c = pow(abs(value), 0.1593017578f);
    return pow((0.8359375f + 18.8515625f * c) / (1.0f + 18.6875f * c), 78.84375f);
}

float3 linear_to_st2084(float3 value) {
    return float3(
        linear_to_st2084_channel(value.r),
        linear_to_st2084_channel(value.g),
        linear_to_st2084_channel(value.b));
}

float encode_p010_unorm(float code_value) {
    float packed = floor(code_value + 0.5f) * 64.0f;
    return packed / 65535.0f;
}

float main(float4 position : SV_Position) : SV_Target {
    uint2 pixel = uint2(position.xy);
    float3 rgb = gSource.Load(int3(pixel, 0)).rgb;
    rgb = saturate(rec709_to_rec2020(rgb) * kSourceToPqScale);
    rgb = linear_to_st2084(rgb);

    float y = kKr * rgb.r + kKg * rgb.g + kKb * rgb.b;
    return encode_p010_unorm(clamp(64.0f + y * 876.0f, 64.0f, 940.0f));
}
)";

const std::string k_p010_chroma_pixel_shader = R"(
Texture2D<float4> gSource : register(t0);

static const float kSourceToPqScale = 80.0f / 10000.0f;
static const float kKr = 0.2627f;
static const float kKb = 0.0593f;
static const float kKg = 1.0f - kKr - kKb;
static const float kCbDenominator = 2.0f * (1.0f - kKb);
static const float kCrDenominator = 2.0f * (1.0f - kKr);

float3 rec709_to_rec2020(float3 value) {
    float r = dot(value, float3(0.62740389593469903f, 0.32928303837788370f, 0.043313065687417225f));
    float g = dot(value, float3(0.069097289358232075f, 0.91954039507545871f, 0.011362315566309178f));
    float b = dot(value, float3(0.016391438875150280f, 0.088013307877225749f, 0.89559525324762401f));
    return float3(r, g, b);
}

float linear_to_st2084_channel(float value) {
    float c = pow(abs(value), 0.1593017578f);
    return pow((0.8359375f + 18.8515625f * c) / (1.0f + 18.6875f * c), 78.84375f);
}

float3 linear_to_st2084(float3 value) {
    return float3(
        linear_to_st2084_channel(value.r),
        linear_to_st2084_channel(value.g),
        linear_to_st2084_channel(value.b));
}

float encode_p010_unorm(float code_value) {
    float packed = floor(code_value + 0.5f) * 64.0f;
    return packed / 65535.0f;
}

float2 main(float4 position : SV_Position) : SV_Target {
    uint2 pixel = uint2(position.xy) * 2u;
    float3 rgb = (
        gSource.Load(int3(pixel, 0)).rgb +
        gSource.Load(int3(pixel + uint2(1u, 0u), 0)).rgb +
        gSource.Load(int3(pixel + uint2(0u, 1u), 0)).rgb +
        gSource.Load(int3(pixel + uint2(1u, 1u), 0)).rgb) * 0.25f;

    rgb = saturate(rec709_to_rec2020(rgb) * kSourceToPqScale);
    rgb = linear_to_st2084(rgb);

    float y = kKr * rgb.r + kKg * rgb.g + kKb * rgb.b;
    float cb = ((rgb.b - y) / kCbDenominator) + 0.5f;
    float cr = ((rgb.r - y) / kCrDenominator) + 0.5f;

    return float2(
        encode_p010_unorm(clamp(64.0f + cb * 896.0f, 64.0f, 960.0f)),
        encode_p010_unorm(clamp(64.0f + cr * 896.0f, 64.0f, 960.0f)));
}
)";

auto create_vertex_shader(ID3D11Device* device, const std::string& shader_code)
    -> std::expected<wil::com_ptr<ID3D11VertexShader>, std::string> {
  auto blob_result = Utils::Graphics::D3D::compile_shader(shader_code, "main", "vs_4_0");
  if (!blob_result) {
    return std::unexpected(blob_result.error());
  }

  wil::com_ptr<ID3D11VertexShader> shader;
  HRESULT hr =
      device->CreateVertexShader(blob_result->get()->GetBufferPointer(),
                                 blob_result->get()->GetBufferSize(), nullptr, shader.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create HDR vertex shader, HRESULT: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }
  return shader;
}

auto create_pixel_shader(ID3D11Device* device, const std::string& shader_code,
                         std::string_view name)
    -> std::expected<wil::com_ptr<ID3D11PixelShader>, std::string> {
  auto blob_result = Utils::Graphics::D3D::compile_shader(shader_code, "main", "ps_4_0");
  if (!blob_result) {
    return std::unexpected(
        std::format("Failed to compile {} shader: {}", name, blob_result.error()));
  }

  wil::com_ptr<ID3D11PixelShader> shader;
  HRESULT hr =
      device->CreatePixelShader(blob_result->get()->GetBufferPointer(),
                                blob_result->get()->GetBufferSize(), nullptr, shader.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create {} shader, HRESULT: 0x{:08X}", name,
                                       static_cast<unsigned>(hr)));
  }
  return shader;
}

auto create_p010_render_targets(ConverterContext& converter) -> std::expected<void, std::string> {
  D3D11_TEXTURE2D_DESC texture_desc{};
  texture_desc.Width = converter.width;
  texture_desc.Height = converter.height;
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = DXGI_FORMAT_P010;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  HRESULT hr =
      converter.device3->CreateTexture2D(&texture_desc, nullptr, converter.p010_texture.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create HDR P010 texture, HRESULT: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  D3D11_RENDER_TARGET_VIEW_DESC1 luma_desc{};
  luma_desc.Format = DXGI_FORMAT_R16_UNORM;
  luma_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  luma_desc.Texture2D.MipSlice = 0;
  luma_desc.Texture2D.PlaneSlice = 0;
  hr = converter.device3->CreateRenderTargetView1(converter.p010_texture.get(), &luma_desc,
                                                  converter.luma_rtv.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create HDR P010 luma render target, HRESULT: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  D3D11_RENDER_TARGET_VIEW_DESC1 chroma_desc{};
  chroma_desc.Format = DXGI_FORMAT_R16G16_UNORM;
  chroma_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  chroma_desc.Texture2D.MipSlice = 0;
  chroma_desc.Texture2D.PlaneSlice = 1;
  hr = converter.device3->CreateRenderTargetView1(converter.p010_texture.get(), &chroma_desc,
                                                  converter.chroma_rtv.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create HDR P010 chroma render target, HRESULT: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  return {};
}

auto ensure_source_srv(ConverterContext& converter, ID3D11Texture2D* source_texture)
    -> std::expected<void, std::string> {
  if (converter.source_texture.get() == source_texture && converter.source_srv) {
    return {};
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  source_texture->GetDesc(&source_desc);
  if (source_desc.Width != converter.width || source_desc.Height != converter.height ||
      source_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
    return std::unexpected(std::format(
        "Unexpected HDR source texture: {}x{}, format={} (expected {}x{} R16G16B16A16_FLOAT)",
        source_desc.Width, source_desc.Height, std::to_underlying(source_desc.Format),
        converter.width, converter.height));
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Format = source_desc.Format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MostDetailedMip = 0;
  srv_desc.Texture2D.MipLevels = 1;

  wil::com_ptr<ID3D11ShaderResourceView> srv;
  HRESULT hr = converter.device3->CreateShaderResourceView(source_texture, &srv_desc, srv.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create HDR source SRV, HRESULT: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  converter.source_texture = source_texture;
  converter.source_srv = std::move(srv);
  return {};
}

auto render_luma_pass(ConverterContext& converter, ID3D11DeviceContext* context) -> void {
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(converter.width);
  viewport.Height = static_cast<float>(converter.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  ID3D11RenderTargetView* render_target = converter.luma_rtv.get();
  ID3D11ShaderResourceView* source_srv = converter.source_srv.get();

  context->RSSetViewports(1, &viewport);
  context->OMSetRenderTargets(1, &render_target, nullptr);
  context->IASetInputLayout(nullptr);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(converter.fullscreen_vertex_shader.get(), nullptr, 0);
  context->PSSetShader(converter.luma_pixel_shader.get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &source_srv);
  context->Draw(3, 0);
}

auto render_chroma_pass(ConverterContext& converter, ID3D11DeviceContext* context) -> void {
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(converter.width / 2);
  viewport.Height = static_cast<float>(converter.height / 2);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  ID3D11RenderTargetView* render_target = converter.chroma_rtv.get();
  ID3D11ShaderResourceView* source_srv = converter.source_srv.get();

  context->RSSetViewports(1, &viewport);
  context->OMSetRenderTargets(1, &render_target, nullptr);
  context->IASetInputLayout(nullptr);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(converter.fullscreen_vertex_shader.get(), nullptr, 0);
  context->PSSetShader(converter.chroma_pixel_shader.get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &source_srv);
  context->Draw(3, 0);
}

auto clear_bindings(ID3D11DeviceContext* context) -> void {
  ID3D11ShaderResourceView* null_srvs[] = {nullptr};
  ID3D11RenderTargetView* null_rtvs[] = {nullptr};

  context->PSSetShaderResources(0, 1, null_srvs);
  context->OMSetRenderTargets(1, null_rtvs, nullptr);
  context->VSSetShader(nullptr, nullptr, 0);
  context->PSSetShader(nullptr, nullptr, 0);
}

auto create_converter(ID3D11Device* device, std::uint32_t width, std::uint32_t height)
    -> std::expected<ConverterContext, std::string> {
  if (!device) {
    return std::unexpected("D3D device is required for HDR conversion");
  }
  if (width == 0 || height == 0 || (width % 2) != 0 || (height % 2) != 0) {
    return std::unexpected("HDR conversion requires even, non-zero frame dimensions");
  }

  ConverterContext converter;
  converter.width = width;
  converter.height = height;

  HRESULT hr = device->QueryInterface(__uuidof(ID3D11Device3),
                                      reinterpret_cast<void**>(converter.device3.put()));
  if (FAILED(hr)) {
    return std::unexpected("HDR conversion requires ID3D11Device3 plane render target support");
  }

  auto vertex_shader_result = create_vertex_shader(device, k_fullscreen_vertex_shader);
  if (!vertex_shader_result) {
    return std::unexpected(vertex_shader_result.error());
  }
  converter.fullscreen_vertex_shader = std::move(*vertex_shader_result);

  auto luma_shader_result = create_pixel_shader(device, k_p010_luma_pixel_shader, "HDR P010 luma");
  if (!luma_shader_result) {
    return std::unexpected(luma_shader_result.error());
  }
  converter.luma_pixel_shader = std::move(*luma_shader_result);

  auto chroma_shader_result =
      create_pixel_shader(device, k_p010_chroma_pixel_shader, "HDR P010 chroma");
  if (!chroma_shader_result) {
    return std::unexpected(chroma_shader_result.error());
  }
  converter.chroma_pixel_shader = std::move(*chroma_shader_result);

  auto render_target_result = create_p010_render_targets(converter);
  if (!render_target_result) {
    return std::unexpected(render_target_result.error());
  }

  Logger().info("HDR shader converter created for scRGB->P010 conversion");
  return converter;
}

auto convert_frame(ConverterContext& converter, ID3D11DeviceContext* context,
                   ID3D11Texture2D* source_texture)
    -> std::expected<ID3D11Texture2D*, std::string> {
  if (!context || !source_texture) {
    return std::unexpected("HDR conversion requires valid D3D context and source texture");
  }
  if (!converter.device3 || !converter.p010_texture || !converter.luma_rtv ||
      !converter.chroma_rtv || !converter.fullscreen_vertex_shader ||
      !converter.luma_pixel_shader || !converter.chroma_pixel_shader) {
    return std::unexpected("HDR conversion resources are not initialized");
  }

  auto source_srv_result = ensure_source_srv(converter, source_texture);
  if (!source_srv_result) {
    return std::unexpected(source_srv_result.error());
  }

  render_luma_pass(converter, context);
  render_chroma_pass(converter, context);
  clear_bindings(context);

  return converter.p010_texture.get();
}

}  // namespace Utils::Media::HdrConvert
