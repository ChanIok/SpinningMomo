#include "utils/media/video_convert.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/d3d11_4.hpp"

#include "utils/graphics/d3d.hpp"
#include "utils/logger/logger.hpp"

namespace utils::media::video_convert {

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

// WGC SDR BGRA 的 RGB 码值转为 BT.709 limited NV12；2x2 色度平均与 HDR 路径一致
const std::string k_nv12_luma_pixel_shader = R"(
Texture2D<float4> gSource : register(t0);
float main(float4 position : SV_Position) : SV_Target {
    float3 rgb = gSource.Load(int3(uint2(position.xy), 0)).rgb;
    return (16.0f + 219.0f * dot(rgb, float3(0.2126f, 0.7152f, 0.0722f))) / 255.0f;
}
)";

const std::string k_nv12_chroma_pixel_shader = R"(
Texture2D<float4> gSource : register(t0);
float2 main(float4 position : SV_Position) : SV_Target {
    uint2 p = uint2(position.xy) * 2u;
    float3 rgb = (gSource.Load(int3(p,0)).rgb + gSource.Load(int3(p+uint2(1,0),0)).rgb
                + gSource.Load(int3(p+uint2(0,1),0)).rgb + gSource.Load(int3(p+uint2(1,1),0)).rgb) * 0.25f;
    float y = dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
    return (128.0f + 224.0f * float2((rgb.b-y)/1.8556f, (rgb.r-y)/1.5748f)) / 255.0f;
}
)";

// 编译全屏三角形顶点着色器
auto create_vertex_shader(ID3D11Device* device, const std::string& shader_code)
    -> std::expected<wil::com_ptr<ID3D11VertexShader>, std::string> {
  auto blob_result = utils::graphics::d3d::compile_shader(shader_code, "main", "vs_4_0");
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

// 编译格式转换像素着色器
auto create_pixel_shader(ID3D11Device* device, const std::string& shader_code,
                         std::string_view name)
    -> std::expected<wil::com_ptr<ID3D11PixelShader>, std::string> {
  auto blob_result = utils::graphics::d3d::compile_shader(shader_code, "main", "ps_4_0");
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

// 创建单份两平面（NV12 或 P010）目标渲染纹理及独立平面 RTV
auto create_target(ConverterContext& converter) -> std::expected<ConversionTarget, std::string> {
  ConversionTarget target;

  // 配置两平面渲染目标纹理描述符
  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = converter.width;
  desc.Height = converter.height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.Format = converter.hdr ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  auto hr = converter.device3->CreateTexture2D(&desc, nullptr, target.texture.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Create conversion texture: 0x{:08X}", unsigned(hr)));
  }

  // 创建亮度平面（Plane 0: Y）的 RTV
  D3D11_RENDER_TARGET_VIEW_DESC1 view{};
  view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  view.Format = converter.hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;

  hr = converter.device3->CreateRenderTargetView1(target.texture.get(), &view, target.luma.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Create luma RTV: 0x{:08X}", unsigned(hr)));
  }

  // 创建色度平面（Plane 1: UV）的 RTV
  view.Texture2D.PlaneSlice = 1;
  view.Format = converter.hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;

  hr = converter.device3->CreateRenderTargetView1(target.texture.get(), &view, target.chroma.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Create chroma RTV: 0x{:08X}", unsigned(hr)));
  }

  return target;
}

// 确保并复用源纹理的 SRV 视图，校验输入尺寸与像素格式
auto ensure_source_srv(ConverterContext& converter, ID3D11Texture2D* source_texture)
    -> std::expected<void, std::string> {
  // 源纹理句柄未发生变化时直接复用已创建的 SRV
  if (converter.source_texture.get() == source_texture && converter.source_srv) {
    return {};
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  source_texture->GetDesc(&source_desc);

  // 严格核对格式与尺寸是否与预期一致
  if (source_desc.Width != converter.width || source_desc.Height != converter.height ||
      source_desc.Format !=
          (converter.hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM)) {
    return std::unexpected(
        std::format("Unexpected Video source texture: {}x{}, format={} (expected {}x{} matching "
                    "capture format)",
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
    return std::unexpected(std::format("Failed to create Video source SRV, HRESULT: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  converter.source_texture = source_texture;
  converter.source_srv = std::move(srv);
  return {};
}

// 执行亮度 Pass 渲染：将全分辨率 RGB 映射至 Y 平面
auto render_luma_pass(ConverterContext& converter, ID3D11DeviceContext* context,
                      ConversionTarget& target) -> void {
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(converter.width);
  viewport.Height = static_cast<float>(converter.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  ID3D11RenderTargetView* render_target = target.luma.get();
  ID3D11ShaderResourceView* source_srv = converter.source_srv.get();

  context->RSSetViewports(1, &viewport);
  context->OMSetRenderTargets(1, &render_target, nullptr);
  context->IASetInputLayout(nullptr);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(converter.fullscreen_vertex_shader.get(), nullptr, 0);
  context->PSSetShader(converter.luma_pixel_shader.get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &source_srv);

  // 绘制全屏三角形
  context->Draw(3, 0);
}

// 执行色度 Pass 渲染：2x2 像素下采样并映射至半分辨率 UV 平面
auto render_chroma_pass(ConverterContext& converter, ID3D11DeviceContext* context,
                        ConversionTarget& target) -> void {
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(converter.width / 2);
  viewport.Height = static_cast<float>(converter.height / 2);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  ID3D11RenderTargetView* render_target = target.chroma.get();
  ID3D11ShaderResourceView* source_srv = converter.source_srv.get();

  context->RSSetViewports(1, &viewport);
  context->OMSetRenderTargets(1, &render_target, nullptr);
  context->IASetInputLayout(nullptr);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(converter.fullscreen_vertex_shader.get(), nullptr, 0);
  context->PSSetShader(converter.chroma_pixel_shader.get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &source_srv);

  // 绘制全屏三角形
  context->Draw(3, 0);
}

// 解除管线上的 SRV、RTV 和着色器绑定，避免后续渲染状态冲突
auto clear_bindings(ID3D11DeviceContext* context) -> void {
  ID3D11ShaderResourceView* null_srvs[] = {nullptr};
  ID3D11RenderTargetView* null_rtvs[] = {nullptr};

  context->PSSetShaderResources(0, 1, null_srvs);
  context->OMSetRenderTargets(1, null_rtvs, nullptr);
  context->VSSetShader(nullptr, nullptr, 0);
  context->PSSetShader(nullptr, nullptr, 0);
}

// 初始化视频格式转换器：编译全屏顶点与亮度/色度像素着色器
auto create_converter(ID3D11Device* device, std::uint32_t width, std::uint32_t height, bool hdr)
    -> std::expected<ConverterContext, std::string> {
  if (!device) {
    return std::unexpected("D3D device is required for Video conversion");
  }
  if (width == 0 || height == 0 || (width % 2) != 0 || (height % 2) != 0) {
    return std::unexpected("Video conversion requires even, non-zero frame dimensions");
  }

  ConverterContext converter;
  converter.hdr = hdr;
  converter.width = width;
  converter.height = height;

  // 检查 ID3D11Device3 接口支持（需要 Plane RTV 特性）
  HRESULT hr = device->QueryInterface(__uuidof(ID3D11Device3),
                                      reinterpret_cast<void**>(converter.device3.put()));
  if (FAILED(hr)) {
    return std::unexpected("Video conversion requires ID3D11Device3 plane render target support");
  }

  // 编译全屏三角形顶点着色器
  auto vertex_shader_result = create_vertex_shader(device, k_fullscreen_vertex_shader);
  if (!vertex_shader_result) {
    return std::unexpected(vertex_shader_result.error());
  }
  converter.fullscreen_vertex_shader = std::move(*vertex_shader_result);

  // 编译亮度平面着色器（NV12 或 P010）
  auto luma_shader_result = create_pixel_shader(
      device, hdr ? k_p010_luma_pixel_shader : k_nv12_luma_pixel_shader, "video luma");
  if (!luma_shader_result) {
    return std::unexpected(luma_shader_result.error());
  }
  converter.luma_pixel_shader = std::move(*luma_shader_result);

  // 编译色度平面着色器（NV12 或 P010）
  auto chroma_shader_result = create_pixel_shader(
      device, hdr ? k_p010_chroma_pixel_shader : k_nv12_chroma_pixel_shader, "video chroma");
  if (!chroma_shader_result) {
    return std::unexpected(chroma_shader_result.error());
  }
  converter.chroma_pixel_shader = std::move(*chroma_shader_result);

  return converter;
}

// 执行帧格式转换：绑定源纹理 SRV → 渲染亮度 Pass → 渲染色度 Pass → 清理渲染管线绑定
auto convert_frame(ConverterContext& converter, ID3D11DeviceContext* context,
                   ID3D11Texture2D* source_texture, ConversionTarget& target)
    -> std::expected<void, std::string> {
  // 绑定或复用源纹理的 SRV
  auto source = ensure_source_srv(converter, source_texture);
  if (!source) {
    return std::unexpected(source.error());
  }

  // 分别渲染 Y 与 UV 平面
  render_luma_pass(converter, context, target);
  render_chroma_pass(converter, context, target);

  // 恢复管线状态
  clear_bindings(context);
  return {};
}

}  // namespace utils::media::video_convert
