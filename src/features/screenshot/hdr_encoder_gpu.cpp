module;

#include <d3d11.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <windows.h>

module Features.Screenshot.HdrEncoder;

import std;
import Utils.Graphics.D3D;
import Utils.Logger;

namespace Features::Screenshot::HdrEncoder {

namespace GpuPreprocess {

// 本文件把原先最重的 CPU 逐像素路径搬到 GPU：
// 1) histogram pass：从 R16G16B16A16_FLOAT 捕获纹理统计亮度直方图；
// 2) CPU 从 4096 桶直方图中求 P99-ish 内容峰值；
// 3) preprocess pass：用同一套 ICtCp tone-map 数学生成 SDR 底图和 Ultra HDR 的 HDR half 平面。
// libultrahdr 仍保留在 CPU 侧，因为它本身负责 JPEG/增益图封装。
constexpr std::uint32_t kThreadGroupSizeX = 16;
constexpr std::uint32_t kThreadGroupSizeY = 16;
constexpr std::uint32_t kChannelsPerPixel = 4;
constexpr std::uint32_t kSdrBytesPerPixel = 4;
constexpr std::uint32_t kHdrBytesPerPixel = 8;
constexpr std::uint32_t kHistogramBinCount = 4096;
constexpr float kScRgbReferenceWhiteNits = 80.0f;
constexpr float kMaxPqLinearValue = 10000.0f / kScRgbReferenceWhiteNits;
constexpr float kSdrBasePeakLinearValue = 1.5f;
constexpr double kContentPeakPercentile = 0.9994;

constexpr float kPqC1 = 107.0f / 128.0f;
constexpr float kPqC2 = 2413.0f / 128.0f;
constexpr float kPqC3 = 2392.0f / 128.0f;
constexpr float kPqM = 2523.0f / 32.0f;
constexpr float kPqN = 1305.0f / 8192.0f;

struct HistogramConstants {
  // 捕获纹理尺寸；shader 用它过滤越界线程。
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  // 亮度直方图的线性上限：scRGB 80 nit 参考白下的 10000 nit。
  float max_pq_linear_value = kMaxPqLinearValue;
  std::uint32_t histogram_bin_count = kHistogramBinCount;
};

struct PreprocessConstants {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  // CPU 从 GPU histogram 得到的内容峰值，转成 PQ 码值后传给 tone-map。
  float content_peak_pq = 0.0f;
  // SDR 底图使用的固定参考肩线，保持旧 CPU 算法的观感边界。
  float sdr_peak_pq = 0.0f;
};

// 亮度直方图 pass。
// 输入：Windows Graphics Capture 产出的 scRGB-like R16G16B16A16_FLOAT 纹理。
// 输出：4096 个 uint 桶。这里故意只统计亮度，不做 tone-map，读回的数据很小。
const std::string kHistogramComputeShader = R"(
Texture2D<float4> gSource : register(t0);
RWStructuredBuffer<uint> gHistogram : register(u0);

cbuffer HistogramConstants : register(b0) {
    uint gWidth;
    uint gHeight;
    float gMaxPqLinearValue;
    uint gHistogramBinCount;
};

float sanitize_positive(float value) {
    if (!isfinite(value) || value <= 0.0f) {
        return 0.0f;
    }
    return value;
}

float rec709_luminance(float3 rgb) {
    return 0.2126390040f * rgb.x + 0.7151686549f * rgb.y + 0.0721923187f * rgb.z;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (dispatchThreadId.x >= gWidth || dispatchThreadId.y >= gHeight) {
        return;
    }

    float4 pixel = gSource.Load(int3(dispatchThreadId.xy, 0));
    float3 rgb = float3(
        sanitize_positive(pixel.r),
        sanitize_positive(pixel.g),
        sanitize_positive(pixel.b));
    float luminance = clamp(rec709_luminance(rgb), 0.0f, gMaxPqLinearValue);
    // 将线性亮度均匀映射到固定桶数；最终分位选择在 CPU 侧完成，避免 GPU 上做复杂归约。
    uint bin = min((uint)(luminance / gMaxPqLinearValue * (float)gHistogramBinCount),
                   gHistogramBinCount - 1);
    InterlockedAdd(gHistogram[bin], 1);
}
)";

// 主预处理 pass。
// 输入：原始 HDR 捕获纹理 + 上一步算出的内容峰值。
// 输出：
// - gSdrPixels：每像素 1 个 uint，按内存小端布局打包为 RGBA8888，直接交给 libultrahdr 的 SDR 图；
// - gHdrPixels：每像素 1 个 uint2，打包 4 个 half（R/G、B/A），直接交给 libultrahdr 的 HDR 图。
// 这两个 structured buffer 都是紧排数据，CPU readback 后不需要处理纹理 RowPitch。
const std::string kPreprocessComputeShader = R"(
Texture2D<float4> gSource : register(t0);
RWStructuredBuffer<uint> gSdrPixels : register(u0);
RWStructuredBuffer<uint2> gHdrPixels : register(u1);

cbuffer PreprocessConstants : register(b0) {
    uint gWidth;
    uint gHeight;
    float gContentPeakPq;
    float gSdrPeakPq;
};

static const float kScRgbToUltraHdrLinearScale = 80.0f / 203.0f;
static const float kMaxUltraHdrLinearValue = 10000.0f / 203.0f;
static const float kPqC1 = 107.0f / 128.0f;
static const float kPqC2 = 2413.0f / 128.0f;
static const float kPqC3 = 2392.0f / 128.0f;
static const float kPqM = 2523.0f / 32.0f;
static const float kPqN = 1305.0f / 8192.0f;
static const float kPqInvM = 1.0f / kPqM;
static const float kPqInvN = 1.0f / kPqN;
static const float kMaxPqLinearValue = 10000.0f / 80.0f;

// WGC 偶尔可能给出 NaN/Inf，保存图片时统一当作黑色/有限值处理。
float sanitize_linear(float value) {
    if (!isfinite(value)) {
        return 0.0f;
    }
    return value;
}

float sanitize_positive(float value) {
    return max(sanitize_linear(value), 0.0f);
}

float3 mul_rec709_to_xyz(float3 value) {
    return float3(
        0.4123907983f * value.x + 0.3575843275f * value.y + 0.1804807931f * value.z,
        0.2126390040f * value.x + 0.7151686549f * value.y + 0.0721923187f * value.z,
        0.0193308182f * value.x + 0.1191947833f * value.y + 0.9505321383f * value.z);
}

float3 mul_xyz_to_rec709(float3 value) {
    return float3(
        3.2409698963f * value.x + -1.5373831987f * value.y + -0.4986107647f * value.z,
        -0.9692436457f * value.x + 1.8759675026f * value.y + 0.0415550582f * value.z,
        0.0556300804f * value.x + -0.2039769590f * value.y + 1.0569715500f * value.z);
}

// 以下矩阵与旧 CPU 路径一致：Rec.709 linear RGB -> XYZ -> LMS -> PQ -> ICtCp。
// tone-map 只动 ICtCp 的 I 分量，再按比例压缩色度，目标是避免 RGB 逐通道 clamp 的偏色。
float3 mul_xyz_to_lms(float3 value) {
    return float3(
        0.3592f * value.x + 0.6976f * value.y + -0.0358f * value.z,
        -0.1922f * value.x + 1.1004f * value.y + 0.0755f * value.z,
        0.0070f * value.x + 0.0749f * value.y + 0.8434f * value.z);
}

float3 mul_lms_to_xyz(float3 value) {
    return float3(
        2.0701800567f * value.x + -1.3264568761f * value.y + 0.2066160068f * value.z,
        0.3649882500f * value.x + 0.6804673629f * value.y + -0.0454217531f * value.z,
        -0.0495955422f * value.x + -0.0494211612f * value.y + 1.1879959417f * value.z);
}

float3 mul_lms_pq_to_ictcp(float3 value) {
    return float3(
        0.5000f * value.x + 0.5000f * value.y + 0.0000f * value.z,
        1.6137f * value.x + -3.3234f * value.y + 1.7097f * value.z,
        4.3780f * value.x + -4.2455f * value.y + -0.1325f * value.z);
}

float3 mul_ictcp_to_lms_pq(float3 value) {
    return float3(
        1.0f * value.x + 0.0086051457f * value.y + 0.1110356045f * value.z,
        1.0f * value.x + -0.0086051457f * value.y + -0.1110356045f * value.z,
        1.0f * value.x + 0.5600488596f * value.y + -0.3206374702f * value.z);
}

float linear_to_pq(float value) {
    if (!isfinite(value) || value == 0.0f) {
        return 0.0f;
    }

    float sign_value = value < 0.0f ? -1.0f : 1.0f;
    float normalized = clamp(abs(value) / kMaxPqLinearValue, 0.0f, 1.0f);
    float powered = pow(normalized, kPqN);
    float encoded = pow((kPqC1 + kPqC2 * powered) / (1.0f + kPqC3 * powered), kPqM);
    return sign_value * encoded;
}

// PQ 反函数用于从 ICtCp 回到 scene-linear RGB。
float pq_to_linear(float value) {
    if (!isfinite(value) || value == 0.0f) {
        return 0.0f;
    }

    float sign_value = value < 0.0f ? -1.0f : 1.0f;
    float encoded = pow(abs(value), kPqInvM);
    float denominator = kPqC2 - kPqC3 * encoded;
    if (denominator <= 0.0f) {
        return 0.0f;
    }

    float normalized = max(encoded - kPqC1, 0.0f) / denominator;
    return sign_value * pow(normalized, kPqInvN) * kMaxPqLinearValue;
}

float3 rec709_to_ictcp(float3 rgb) {
    float3 xyz = mul_rec709_to_xyz(rgb);
    float3 lms = mul_xyz_to_lms(xyz);
    lms.x = linear_to_pq(max(lms.x, 0.0f));
    lms.y = linear_to_pq(max(lms.y, 0.0f));
    lms.z = linear_to_pq(max(lms.z, 0.0f));
    return mul_lms_pq_to_ictcp(lms);
}

float3 ictcp_to_rec709(float3 ictcp) {
    float3 lms_pq = mul_ictcp_to_lms_pq(ictcp);
    float3 lms = float3(
        pq_to_linear(lms_pq.x),
        pq_to_linear(lms_pq.y),
        pq_to_linear(lms_pq.z));
    float3 xyz = mul_lms_to_xyz(lms);
    return mul_xyz_to_rec709(xyz);
}

// 将 ICtCp 的强度分量从内容峰值压到 SDR 参考峰值。公式保持旧 CPU 实现，
// 这样 GPU 化只改变执行位置，不改变导出风格。
float tone_map_intensity(float input_intensity) {
    if (input_intensity <= 0.0f || gContentPeakPq <= gSdrPeakPq) {
        return input_intensity;
    }

    float shoulder_a = gSdrPeakPq / (gContentPeakPq * gContentPeakPq);
    float shoulder_b = 1.0f / gSdrPeakPq;
    float mapped = input_intensity * (1.0f + shoulder_a * input_intensity) /
                   (1.0f + shoulder_b * input_intensity);
    return clamp(mapped, 0.0f, 1.0f);
}

// SDR 底图写入 libultrahdr 前已经是 sRGB transfer，所以这里在 GPU 侧完成 OETF。
float srgb_oetf(float value) {
    value = clamp(value, 0.0f, 1.0f);
    if (value <= 0.0031308f) {
        return value * 12.92f;
    }
    return 1.055f * pow(value, 1.0f / 2.4f) - 0.055f;
}

uint float_to_u8(float value) {
    return (uint)floor(clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// structured buffer 写 uint，CPU 读回后按 byte vector 解释；Windows/x86 小端下内存顺序为 RGBA。
uint pack_rgba8(uint r, uint g, uint b, uint a) {
    return (r & 0xFFu) | ((g & 0xFFu) << 8) | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}

float sanitize_hdr_half_value(float value) {
    if (!isfinite(value)) {
        return value > 0.0f ? kMaxUltraHdrLinearValue : 0.0f;
    }
    return clamp(value, 0.0f, kMaxUltraHdrLinearValue);
}

// libultrahdr HDR 图层要求 half float。f32tof16 是 shader model 5 可用的快速量化路径。
uint pack_half2(float a, float b) {
    uint half_a = f32tof16(sanitize_hdr_half_value(a)) & 0xFFFFu;
    uint half_b = f32tof16(sanitize_hdr_half_value(b)) & 0xFFFFu;
    return half_a | (half_b << 16);
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (dispatchThreadId.x >= gWidth || dispatchThreadId.y >= gHeight) {
        return;
    }

    uint index = dispatchThreadId.y * gWidth + dispatchThreadId.x;
    float4 source = gSource.Load(int3(dispatchThreadId.xy, 0));
    float3 rgb = float3(
        sanitize_positive(source.r),
        sanitize_positive(source.g),
        sanitize_positive(source.b));

    // SDR 兼容底图：如果内容峰值没超过 SDR 肩线，就直接线性转 sRGB，避免暗部/普通 SDR 场景被无谓改动。
    float3 sdr_linear = rgb;
    if (gContentPeakPq > gSdrPeakPq) {
        float3 ictcp = rec709_to_ictcp(rgb);
        float input_intensity = max(ictcp.x, 0.0f);
        if (input_intensity > 0.0f) {
            float mapped_intensity = tone_map_intensity(input_intensity);
            float chroma_scale = mapped_intensity > 0.0f
                ? clamp(min(input_intensity / mapped_intensity,
                            mapped_intensity / input_intensity), 0.0f, 1.0f)
                : 0.0f;
            ictcp.x = mapped_intensity;
            ictcp.y *= chroma_scale;
            ictcp.z *= chroma_scale;
        } else {
            ictcp = float3(0.0f, 0.0f, 0.0f);
        }
        sdr_linear = ictcp_to_rec709(ictcp);
    }

    uint red = float_to_u8(srgb_oetf(max(sdr_linear.x, 0.0f)));
    uint green = float_to_u8(srgb_oetf(max(sdr_linear.y, 0.0f)));
    uint blue = float_to_u8(srgb_oetf(max(sdr_linear.z, 0.0f)));
    gSdrPixels[index] = pack_rgba8(red, green, blue, 255u);

    // HDR 图层：Windows/scRGB 常以 80 nit 为参考白；Ultra HDR raw half 路径用 203 nit 参考白。
    // 这里保持旧 CPU 路径的 80/203 缩放，A 固定为 half(1.0)。
    float3 hdr_rgb = rgb * kScRgbToUltraHdrLinearScale;
    gHdrPixels[index] = uint2(
        pack_half2(hdr_rgb.x, hdr_rgb.y),
        pack_half2(hdr_rgb.z, 1.0f));
}
)";

auto elapsed_ms(std::chrono::steady_clock::time_point start) -> long long {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               start)
      .count();
}

// CPU 侧只需要 PQ 正函数：把 histogram 得到的线性内容峰值转换成 shader 使用的 PQ 峰值。
auto linear_to_pq(float value) -> float {
  if (!std::isfinite(value) || value == 0.0f) {
    return 0.0f;
  }

  float sign = value < 0.0f ? -1.0f : 1.0f;
  float normalized = std::clamp(std::abs(value) / kMaxPqLinearValue, 0.0f, 1.0f);
  float powered = std::pow(normalized, kPqN);
  float encoded = std::pow((kPqC1 + kPqC2 * powered) / (1.0f + kPqC3 * powered), kPqM);
  return sign * encoded;
}

// 从 GPU 读回的亮度直方图里取“最亮的 0.06% 像素覆盖到的桶”作为内容峰值。
// 这种 P99-ish 峰值比 absolute max 稳定，能避免单个高亮噪点让整张 SDR 底图变灰。
auto compute_content_peak_linear(const std::vector<std::uint32_t>& histogram,
                                 std::uint64_t pixel_count) -> float {
  if (histogram.size() != kHistogramBinCount || pixel_count == 0) {
    return kSdrBasePeakLinearValue;
  }

  std::uint64_t highlight_pixel_count = std::max<std::uint64_t>(
      1, static_cast<std::uint64_t>(
             std::ceil(static_cast<double>(pixel_count) * (1.0 - kContentPeakPercentile))));

  std::uint64_t cumulative = 0;
  for (std::size_t i = kHistogramBinCount; i > 0; --i) {
    cumulative += histogram[i - 1];
    if (cumulative >= highlight_pixel_count) {
      float peak = ((static_cast<float>(i) - 0.5f) / static_cast<float>(kHistogramBinCount)) *
                   kMaxPqLinearValue;
      return std::clamp(peak, kSdrBasePeakLinearValue, kMaxPqLinearValue);
    }
  }

  return kSdrBasePeakLinearValue;
}

// 运行时编译 compute shader。截图不是持续每帧渲染路径，首次 HDR 截图的编译成本可接受；
// 后续如果要进一步压低冷启动耗时，可以再引入缓存或预编译字节码。
auto create_compute_shader(ID3D11Device* device, const std::string& shader_code,
                           std::string_view name)
    -> std::expected<wil::com_ptr<ID3D11ComputeShader>, std::string> {
  if (!device) {
    return std::unexpected("D3D device is null");
  }

  auto blob_result = Utils::Graphics::D3D::compile_shader(shader_code, "main", "cs_5_0");
  if (!blob_result) {
    return std::unexpected(
        std::format("Failed to compile {} compute shader: {}", name, blob_result.error()));
  }

  wil::com_ptr<ID3D11ComputeShader> shader;
  HRESULT hr =
      device->CreateComputeShader(blob_result->get()->GetBufferPointer(),
                                  blob_result->get()->GetBufferSize(), nullptr, shader.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create {} compute shader, HRESULT: 0x{:08X}",
                                       name, static_cast<unsigned int>(hr)));
  }
  return shader;
}

// D3D11 constant buffer 的 ByteWidth 必须是 16 字节倍数，所以调用方结构体要显式满足该约束。
template <typename Constants>
auto create_constant_buffer(ID3D11Device* device, const Constants& constants)
    -> std::expected<wil::com_ptr<ID3D11Buffer>, std::string> {
  static_assert(sizeof(Constants) % 16 == 0);
  if (!device) {
    return std::unexpected("D3D device is null");
  }

  D3D11_BUFFER_DESC desc{};
  desc.ByteWidth = static_cast<UINT>(sizeof(Constants));
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

  D3D11_SUBRESOURCE_DATA data{};
  data.pSysMem = &constants;

  wil::com_ptr<ID3D11Buffer> buffer;
  HRESULT hr = device->CreateBuffer(&desc, &data, buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create HDR compute constant buffer, HRESULT: "
                    "0x{:08X}",
                    static_cast<unsigned int>(hr)));
  }
  return buffer;
}

// 捕获纹理本身是 WGC 提供的 R16G16B16A16_FLOAT；这里只建 SRV，不复制源纹理。
auto create_source_srv(ID3D11Device* device, ID3D11Texture2D* texture)
    -> std::expected<wil::com_ptr<ID3D11ShaderResourceView>, std::string> {
  if (!device || !texture) {
    return std::unexpected("Invalid D3D source texture");
  }

  D3D11_TEXTURE2D_DESC texture_desc{};
  texture->GetDesc(&texture_desc);

  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Format = texture_desc.Format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MostDetailedMip = 0;
  srv_desc.Texture2D.MipLevels = 1;

  wil::com_ptr<ID3D11ShaderResourceView> srv;
  HRESULT hr = device->CreateShaderResourceView(texture, &srv_desc, srv.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create HDR source SRV, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }
  return srv;
}

// GPU 输出都走 structured buffer：
// - histogram：uint 桶；
// - SDR：uint 打包 RGBA8；
// - HDR：uint2 打包 RGBA16F。
// 用 buffer 而不是 UAV texture 是为了读回后天然紧排，减少 RowPitch 处理和格式转换。
auto create_structured_uav_buffer(ID3D11Device* device, std::uint32_t element_count,
                                  std::uint32_t element_size, std::string_view name)
    -> std::expected<std::pair<wil::com_ptr<ID3D11Buffer>, wil::com_ptr<ID3D11UnorderedAccessView>>,
                     std::string> {
  if (!device) {
    return std::unexpected("D3D device is null");
  }
  if (element_count == 0 || element_size == 0) {
    return std::unexpected(std::format("{} buffer size is empty", name));
  }
  if (static_cast<std::uint64_t>(element_count) * element_size > std::numeric_limits<UINT>::max()) {
    return std::unexpected(std::format("{} buffer is too large", name));
  }

  D3D11_BUFFER_DESC buffer_desc{};
  buffer_desc.ByteWidth = element_count * element_size;
  buffer_desc.Usage = D3D11_USAGE_DEFAULT;
  buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  buffer_desc.StructureByteStride = element_size;

  wil::com_ptr<ID3D11Buffer> buffer;
  HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create {} buffer, HRESULT: 0x{:08X}", name,
                                       static_cast<unsigned int>(hr)));
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
  uav_desc.Format = DXGI_FORMAT_UNKNOWN;
  uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uav_desc.Buffer.FirstElement = 0;
  uav_desc.Buffer.NumElements = element_count;

  wil::com_ptr<ID3D11UnorderedAccessView> uav;
  hr = device->CreateUnorderedAccessView(buffer.get(), &uav_desc, uav.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create {} UAV, HRESULT: 0x{:08X}", name,
                                       static_cast<unsigned int>(hr)));
  }

  return std::make_pair(std::move(buffer), std::move(uav));
}

// UAV buffer 不能直接 Map 读，必须先 CopyResource 到 staging buffer。
// staging buffer 明确清掉 BindFlags/MiscFlags/StructureByteStride，避免不同驱动对 structured
// staging 的差异。
auto read_buffer_bytes(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* source,
                       std::size_t byte_count)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  if (!device || !context || !source) {
    return std::unexpected("Invalid D3D buffer readback arguments");
  }
  if (byte_count > std::numeric_limits<UINT>::max()) {
    return std::unexpected("Readback buffer is too large");
  }

  D3D11_BUFFER_DESC source_desc{};
  source->GetDesc(&source_desc);
  if (byte_count > source_desc.ByteWidth) {
    return std::unexpected("Readback size exceeds source buffer size");
  }

  D3D11_BUFFER_DESC staging_desc = source_desc;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.BindFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging_desc.MiscFlags = 0;
  staging_desc.StructureByteStride = 0;

  wil::com_ptr<ID3D11Buffer> staging;
  HRESULT hr = device->CreateBuffer(&staging_desc, nullptr, staging.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create HDR readback buffer, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }

  context->CopyResource(staging.get(), source);

  D3D11_MAPPED_SUBRESOURCE mapped{};
  hr = context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to map HDR readback buffer, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }
  auto unmap_on_exit = wil::scope_exit([&] { context->Unmap(staging.get(), 0); });

  std::vector<std::uint8_t> bytes(byte_count);
  std::memcpy(bytes.data(), mapped.pData, byte_count);
  return bytes;
}

// 类型化读回包装：底层仍按 byte 拷贝，外层只负责把紧排 bytes 放进目标 vector。
template <typename Value>
auto read_buffer_values(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* source,
                        std::size_t value_count) -> std::expected<std::vector<Value>, std::string> {
  if (value_count > std::numeric_limits<std::size_t>::max() / sizeof(Value)) {
    return std::unexpected("Readback value count is too large");
  }

  auto bytes_result = read_buffer_bytes(device, context, source, value_count * sizeof(Value));
  if (!bytes_result) {
    return std::unexpected(bytes_result.error());
  }

  std::vector<Value> values(value_count);
  std::memcpy(values.data(), bytes_result->data(), bytes_result->size());
  return values;
}

// D3D11 不允许同一资源同时保持输入/输出绑定。每个 dispatch 后解绑，避免后续 readback 或下一 pass
// 触发警告。
auto clear_compute_bindings(ID3D11DeviceContext* context) -> void {
  ID3D11ShaderResourceView* null_srvs[] = {nullptr};
  ID3D11UnorderedAccessView* null_uavs[] = {nullptr, nullptr};
  ID3D11Buffer* null_cbs[] = {nullptr};
  context->CSSetShaderResources(0, 1, null_srvs);
  context->CSSetUnorderedAccessViews(0, 2, null_uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, null_cbs);
  context->CSSetShader(nullptr, nullptr, 0);
}

// Pass 1：生成亮度直方图并读回 4096 个 uint。
// 这里读回量只有 16KB，真正昂贵的逐像素扫描已经在 GPU 上完成。
auto dispatch_histogram(ID3D11Device* device, ID3D11DeviceContext* context,
                        ID3D11ShaderResourceView* source_srv, std::uint32_t width,
                        std::uint32_t height)
    -> std::expected<std::vector<std::uint32_t>, std::string> {
  auto shader_result = create_compute_shader(device, kHistogramComputeShader, "HDR histogram");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  auto histogram_buffer_result = create_structured_uav_buffer(
      device, kHistogramBinCount, sizeof(std::uint32_t), "HDR histogram");
  if (!histogram_buffer_result) {
    return std::unexpected(histogram_buffer_result.error());
  }
  auto [histogram_buffer, histogram_uav] = std::move(histogram_buffer_result.value());

  constexpr UINT kClearValues[4] = {0, 0, 0, 0};
  context->ClearUnorderedAccessViewUint(histogram_uav.get(), kClearValues);

  HistogramConstants constants{
      .width = width,
      .height = height,
      .max_pq_linear_value = kMaxPqLinearValue,
      .histogram_bin_count = kHistogramBinCount,
  };
  auto constant_buffer_result = create_constant_buffer(device, constants);
  if (!constant_buffer_result) {
    return std::unexpected(constant_buffer_result.error());
  }

  ID3D11ShaderResourceView* srvs[] = {source_srv};
  ID3D11UnorderedAccessView* uavs[] = {histogram_uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 1, srvs);
  context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch((width + kThreadGroupSizeX - 1) / kThreadGroupSizeX,
                    (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY, 1);
  clear_compute_bindings(context);

  return read_buffer_values<std::uint32_t>(device, context, histogram_buffer.get(),
                                           kHistogramBinCount);
}

// Pass 2：根据内容峰值生成 libultrahdr 所需的两张 raw image。
// 这一步取代旧 CPU 路径里的 build_sdr_base_pixels() 和 pack_ultrahdr_hdr_rgb_half_plane()。
auto dispatch_preprocess(ID3D11Device* device, ID3D11DeviceContext* context,
                         ID3D11ShaderResourceView* source_srv, std::uint32_t width,
                         std::uint32_t height, float content_peak_linear)
    -> std::expected<UltraHdrPreparedImages, std::string> {
  auto shader_result = create_compute_shader(device, kPreprocessComputeShader, "HDR preprocess");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(width) * height;
  if (pixel_count_u64 == 0 || pixel_count_u64 > std::numeric_limits<UINT>::max()) {
    return std::unexpected("HDR image dimensions are invalid");
  }
  auto pixel_count = static_cast<std::uint32_t>(pixel_count_u64);

  auto sdr_buffer_result =
      create_structured_uav_buffer(device, pixel_count, kSdrBytesPerPixel, "HDR SDR base");
  if (!sdr_buffer_result) {
    return std::unexpected(sdr_buffer_result.error());
  }
  auto [sdr_buffer, sdr_uav] = std::move(sdr_buffer_result.value());

  auto hdr_buffer_result =
      create_structured_uav_buffer(device, pixel_count, kHdrBytesPerPixel, "HDR half plane");
  if (!hdr_buffer_result) {
    return std::unexpected(hdr_buffer_result.error());
  }
  auto [hdr_buffer, hdr_uav] = std::move(hdr_buffer_result.value());

  PreprocessConstants constants{
      .width = width,
      .height = height,
      .content_peak_pq = linear_to_pq(content_peak_linear),
      .sdr_peak_pq = linear_to_pq(kSdrBasePeakLinearValue),
  };
  auto constant_buffer_result = create_constant_buffer(device, constants);
  if (!constant_buffer_result) {
    return std::unexpected(constant_buffer_result.error());
  }

  ID3D11ShaderResourceView* srvs[] = {source_srv};
  ID3D11UnorderedAccessView* uavs[] = {sdr_uav.get(), hdr_uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 1, srvs);
  context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch((width + kThreadGroupSizeX - 1) / kThreadGroupSizeX,
                    (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY, 1);
  clear_compute_bindings(context);

  // libultrahdr 的 raw image 接口仍吃 CPU 指针，所以最终只读回已经处理好的紧排结果。
  auto sdr_bytes_result = read_buffer_bytes(
      device, context, sdr_buffer.get(), static_cast<std::size_t>(pixel_count) * kSdrBytesPerPixel);
  if (!sdr_bytes_result) {
    return std::unexpected(sdr_bytes_result.error());
  }

  auto hdr_pixels_result = read_buffer_values<std::uint16_t>(
      device, context, hdr_buffer.get(), static_cast<std::size_t>(pixel_count) * kChannelsPerPixel);
  if (!hdr_pixels_result) {
    return std::unexpected(hdr_pixels_result.error());
  }

  return UltraHdrPreparedImages{
      .sdr_pixels = std::move(sdr_bytes_result.value()),
      .hdr_pixels = std::move(hdr_pixels_result.value()),
      .width = width,
      .height = height,
  };
}

}  // namespace GpuPreprocess

// 对外的 HDR GPU 预处理入口。调用方只关心“纹理进，两个 raw image 出”，
// 具体 histogram / peak / tone-map / readback 都收在本文件内。
auto preprocess_texture_for_ultrahdr(ID3D11Texture2D* texture)
    -> std::expected<UltraHdrPreparedImages, std::string> {
  if (!texture) {
    return std::unexpected("Texture cannot be null");
  }

  D3D11_TEXTURE2D_DESC desc{};
  texture->GetDesc(&desc);
  if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
    return std::unexpected(
        std::format("Unexpected HDR texture format: {}", std::to_underlying(desc.Format)));
  }
  if (desc.Width == 0 || desc.Height == 0) {
    return std::unexpected("HDR texture has empty dimensions");
  }

  try {
    wil::com_ptr<ID3D11Device> device;
    texture->GetDevice(device.put());
    THROW_HR_IF_NULL(E_POINTER, device);

    wil::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    THROW_HR_IF_NULL(E_POINTER, context);

    auto source_srv_result = GpuPreprocess::create_source_srv(device.get(), texture);
    if (!source_srv_result) {
      return std::unexpected(source_srv_result.error());
    }

    auto total_start = std::chrono::steady_clock::now();

    // 先生成 histogram，再由 CPU 从小数组中计算分位峰值；这比把整张图读回 CPU 便宜得多。
    auto histogram_start = std::chrono::steady_clock::now();
    auto histogram_result = GpuPreprocess::dispatch_histogram(
        device.get(), context.get(), source_srv_result->get(), desc.Width, desc.Height);
    if (!histogram_result) {
      return std::unexpected(histogram_result.error());
    }

    auto pixel_count = static_cast<std::uint64_t>(desc.Width) * desc.Height;
    float content_peak_linear =
        GpuPreprocess::compute_content_peak_linear(histogram_result.value(), pixel_count);
    Logger().debug("HDR GPU histogram: total={} ms, content_peak_linear={:.4f}",
                   GpuPreprocess::elapsed_ms(histogram_start), content_peak_linear);

    // 使用同一个 source SRV 进入主预处理 pass，输出 SDR/HDR 两块紧排内存。
    auto preprocess_start = std::chrono::steady_clock::now();
    auto prepared_result =
        GpuPreprocess::dispatch_preprocess(device.get(), context.get(), source_srv_result->get(),
                                           desc.Width, desc.Height, content_peak_linear);
    if (!prepared_result) {
      return std::unexpected(prepared_result.error());
    }
    Logger().debug("HDR GPU preprocess: total={} ms", GpuPreprocess::elapsed_ms(preprocess_start));
    Logger().debug("HDR GPU preprocess total: {} ms", GpuPreprocess::elapsed_ms(total_start));

    return std::move(prepared_result.value());
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("HDR GPU preprocess failed: {}", e.what()));
  }
}

}  // namespace Features::Screenshot::HdrEncoder
