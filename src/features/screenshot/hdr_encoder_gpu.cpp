module;

module Features.Screenshot.HdrEncoder;

import std;
import Utils.Graphics.D3D;
import Utils.Logger;
import <d3d11.h>;
import <wil/com.h>;
import <wil/result.h>;
import <wil/resource.h>;
import <windows.h>;

namespace Features::Screenshot::HdrEncoder {

namespace GpuPreprocess {

// 这条链路现在的边界是：
// 1) GPU 负责 HDR->SDR 预处理、RGB gain 计算、tile 范围归约、量化；
// 2) CPU 只负责对归约结果做 clamp 与最小范围展开。
constexpr std::uint32_t kThreadGroupSizeX = 16;
constexpr std::uint32_t kThreadGroupSizeY = 16;
constexpr std::uint32_t kReductionThreadCount = 256;
constexpr std::uint32_t kBaseBytesPerPixel = 4;
constexpr std::uint32_t kHdrBytesPerPixel = 8;
constexpr std::uint32_t kGainMapBytesPerPixel = 4;
constexpr std::uint32_t kGainLog2ElementBytes = 16;
constexpr std::uint32_t kTileMinMaxElementBytes = 8;
constexpr std::uint32_t kHistogramBinCount = 4096;
constexpr float kScRgbReferenceWhiteNits = 80.0f;
constexpr float kUltraHdrReferenceWhiteNits = 203.0f;
constexpr float kScRgbToUltraHdrLinearScale =
    kScRgbReferenceWhiteNits / kUltraHdrReferenceWhiteNits;
constexpr float kMaxUltraHdrLinearValue = 10000.0f / kUltraHdrReferenceWhiteNits;
constexpr float kMaxPqLinearValue = 10000.0f / kScRgbReferenceWhiteNits;
constexpr float kSdrBasePeakLinearValue = 1.5f;
constexpr double kContentPeakPercentile = 0.9994;
constexpr float kGainMapGamma = 1.0f;
constexpr float kGainMapMinClamp = -14.3f;
constexpr float kGainMapMaxClamp = 15.6f;
constexpr float kGainMapMinRange = 0.1f;

constexpr float kPqC1 = 107.0f / 128.0f;
constexpr float kPqC2 = 2413.0f / 128.0f;
constexpr float kPqC3 = 2392.0f / 128.0f;
constexpr float kPqM = 2523.0f / 32.0f;
constexpr float kPqN = 1305.0f / 8192.0f;

struct HistogramConstants {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float max_pq_linear_value = kMaxPqLinearValue;
  std::uint32_t histogram_bin_count = kHistogramBinCount;
};

struct PreprocessConstants {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float content_peak_pq = 0.0f;
  float sdr_peak_pq = 0.0f;
};

struct GainComputeConstants {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t groups_x = 0;
  std::uint32_t _padding = 0;
};

struct ReduceConstants {
  std::uint32_t element_count = 0;
  std::uint32_t _padding0 = 0;
  std::uint32_t _padding1 = 0;
  std::uint32_t _padding2 = 0;
};

struct GainQuantizeConstants {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float min_gain_log2 = 0.0f;
  float max_gain_log2 = 0.0f;
  float gamma = kGainMapGamma;
  float _padding0 = 0.0f;
  float _padding1 = 0.0f;
  float _padding2 = 0.0f;
};

struct StructuredBuffer {
  // structured buffer 读回后天然是紧排数组，不必处理 texture row pitch。
  wil::com_ptr<ID3D11Buffer> buffer;
  wil::com_ptr<ID3D11ShaderResourceView> srv;
  wil::com_ptr<ID3D11UnorderedAccessView> uav;
  std::uint32_t element_count = 0;
  std::uint32_t element_size = 0;
};

struct PreprocessOutputs {
  StructuredBuffer base_bgra8_buffer;
  StructuredBuffer hdr_half_rgba_buffer;
};

struct GainComputeOutputs {
  StructuredBuffer gain_log2_rgb_buffer;
  StructuredBuffer tile_minmax_buffer;
};

struct FloatMinMax {
  float min_value = 0.0f;
  float max_value = 0.0f;
};

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
    uint bin = min((uint)(luminance / gMaxPqLinearValue * (float)gHistogramBinCount),
                   gHistogramBinCount - 1);
    InterlockedAdd(gHistogram[bin], 1);
}
)";

const std::string kPreprocessComputeShader = R"(
Texture2D<float4> gSource : register(t0);
RWStructuredBuffer<uint> gBasePixels : register(u0);
RWStructuredBuffer<uint2> gHdrPixels : register(u1);

cbuffer PreprocessConstants : register(b0) {
    uint gWidth;
    uint gHeight;
    float gContentPeakPq;
    float gSdrPeakPq;
};

static const float kScRgbToUltraHdrLinearScale = 80.0f / 203.0f;
static const float kMaxUltraHdrLinearValue = 10000.0f / 203.0f;
static const float kMaxPqLinearValue = 10000.0f / 80.0f;
static const float kPqC1 = 107.0f / 128.0f;
static const float kPqC2 = 2413.0f / 128.0f;
static const float kPqC3 = 2392.0f / 128.0f;
static const float kPqM = 2523.0f / 32.0f;
static const float kPqN = 1305.0f / 8192.0f;
static const float kPqInvM = 1.0f / kPqM;
static const float kPqInvN = 1.0f / kPqN;

float sanitize_linear(float value) {
    if (!isfinite(value)) {
        return 0.0f;
    }
    return value;
}

float sanitize_positive(float value) {
    return max(sanitize_linear(value), 0.0f);
}

float3 max_zero(float3 value) {
    return max(value, float3(0.0f, 0.0f, 0.0f));
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

// WIC 侧直接吃 BGRA，所以这里按小端内存顺序打包成 BGRA8。
uint pack_bgra8(uint b, uint g, uint r, uint a) {
    return (b & 0xFFu) | ((g & 0xFFu) << 8) | ((r & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}

float sanitize_hdr_half_value(float value) {
    if (!isfinite(value)) {
        return value > 0.0f ? kMaxUltraHdrLinearValue : 0.0f;
    }
    return clamp(value, 0.0f, kMaxUltraHdrLinearValue);
}

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
    sdr_linear = max_zero(sdr_linear);

    uint red = float_to_u8(srgb_oetf(sdr_linear.x));
    uint green = float_to_u8(srgb_oetf(sdr_linear.y));
    uint blue = float_to_u8(srgb_oetf(sdr_linear.z));
    gBasePixels[index] = pack_bgra8(blue, green, red, 255u);

    // HDR raw half 路径仍保持旧实现的 80/203 缩放，确保后续 gain 计算语义一致。
    float3 hdr_rgb = rgb * kScRgbToUltraHdrLinearScale;
    gHdrPixels[index] = uint2(
        pack_half2(hdr_rgb.x, hdr_rgb.y),
        pack_half2(hdr_rgb.z, 1.0f));
}
)";

const std::string kGainComputeShader = R"(
StructuredBuffer<uint> gBasePixels : register(t0);
StructuredBuffer<uint2> gHdrPixels : register(t1);
RWStructuredBuffer<float4> gGainLog2Rgb : register(u0);
RWStructuredBuffer<float2> gTileMinMax : register(u1);

cbuffer GainComputeConstants : register(b0) {
    uint gWidth;
    uint gHeight;
    uint gGroupsX;
    uint gPadding;
};

groupshared float gMinR[256];
groupshared float gMinG[256];
groupshared float gMinB[256];
groupshared float gMaxR[256];
groupshared float gMaxG[256];
groupshared float gMaxB[256];

float srgb_inverse_oetf(float value) {
    value = clamp(value, 0.0f, 1.0f);
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
    return pow((value + 0.055f) / 1.055f, 2.4f);
}

float compute_gain_log2(float sdr_nits, float hdr_nits) {
    float gain = log2((hdr_nits + 1e-7f) / (sdr_nits + 1e-7f));
    if (sdr_nits < 2.0f / 255.0f) {
        gain = min(gain, 2.3f);
    }
    return gain;
}

float unpack_half(uint packed, bool upper) {
    uint bits = upper ? (packed >> 16) : (packed & 0xFFFFu);
    return f16tof32(bits);
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID,
          uint3 groupThreadId : SV_GroupThreadID,
          uint3 groupId : SV_GroupID) {
    uint localIndex = groupThreadId.y * 16u + groupThreadId.x;
    bool inBounds = dispatchThreadId.x < gWidth && dispatchThreadId.y < gHeight;

    float3 gain = float3(0.0f, 0.0f, 0.0f);
    float3 localMin = float3(3.402823466e+38f, 3.402823466e+38f, 3.402823466e+38f);
    float3 localMax = float3(-3.402823466e+38f, -3.402823466e+38f, -3.402823466e+38f);

    if (inBounds) {
        uint pixelIndex = dispatchThreadId.y * gWidth + dispatchThreadId.x;

        uint basePacked = gBasePixels[pixelIndex];
        float sdrB = srgb_inverse_oetf((float)(basePacked & 0xFFu) / 255.0f);
        float sdrG = srgb_inverse_oetf((float)((basePacked >> 8) & 0xFFu) / 255.0f);
        float sdrR = srgb_inverse_oetf((float)((basePacked >> 16) & 0xFFu) / 255.0f);

        uint2 hdrPacked = gHdrPixels[pixelIndex];
        float hdrR = clamp(unpack_half(hdrPacked.x, false), 0.0f, 10000.0f / 203.0f);
        float hdrG = clamp(unpack_half(hdrPacked.x, true), 0.0f, 10000.0f / 203.0f);
        float hdrB = clamp(unpack_half(hdrPacked.y, false), 0.0f, 10000.0f / 203.0f);

        gain.r = compute_gain_log2(sdrR * 203.0f, hdrR * 203.0f);
        gain.g = compute_gain_log2(sdrG * 203.0f, hdrG * 203.0f);
        gain.b = compute_gain_log2(sdrB * 203.0f, hdrB * 203.0f);

        gGainLog2Rgb[pixelIndex] = float4(gain, 0.0f);
        localMin = gain;
        localMax = gain;
    }

    gMinR[localIndex] = localMin.r;
    gMinG[localIndex] = localMin.g;
    gMinB[localIndex] = localMin.b;
    gMaxR[localIndex] = localMax.r;
    gMaxG[localIndex] = localMax.g;
    gMaxB[localIndex] = localMax.b;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 128u; stride > 0u; stride >>= 1u) {
        if (localIndex < stride) {
            uint other = localIndex + stride;
            gMinR[localIndex] = min(gMinR[localIndex], gMinR[other]);
            gMinG[localIndex] = min(gMinG[localIndex], gMinG[other]);
            gMinB[localIndex] = min(gMinB[localIndex], gMinB[other]);
            gMaxR[localIndex] = max(gMaxR[localIndex], gMaxR[other]);
            gMaxG[localIndex] = max(gMaxG[localIndex], gMaxG[other]);
            gMaxB[localIndex] = max(gMaxB[localIndex], gMaxB[other]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (localIndex == 0u) {
        uint tileIndex = groupId.y * gGroupsX + groupId.x;
        float tileMin = min(gMinR[0], min(gMinG[0], gMinB[0]));
        float tileMax = max(gMaxR[0], max(gMaxG[0], gMaxB[0]));
        gTileMinMax[tileIndex] = float2(tileMin, tileMax);
    }
}
)";

const std::string kReduceFloat2MinMaxComputeShader = R"(
StructuredBuffer<float2> gInput : register(t0);
RWStructuredBuffer<float2> gOutput : register(u0);

cbuffer ReduceConstants : register(b0) {
    uint gElementCount;
    uint gPadding0;
    uint gPadding1;
    uint gPadding2;
};

groupshared float gSharedMin[256];
groupshared float gSharedMax[256];

[numthreads(256, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID,
          uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint index = dispatchThreadId.x;
    float2 value = index < gElementCount
        ? gInput[index]
        : float2(3.402823466e+38F, -3.402823466e+38F);

    gSharedMin[groupThreadId.x] = value.x;
    gSharedMax[groupThreadId.x] = value.y;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (groupThreadId.x < stride) {
            gSharedMin[groupThreadId.x] = min(gSharedMin[groupThreadId.x],
                                              gSharedMin[groupThreadId.x + stride]);
            gSharedMax[groupThreadId.x] = max(gSharedMax[groupThreadId.x],
                                              gSharedMax[groupThreadId.x + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupThreadId.x == 0) {
        gOutput[groupId.x] = float2(gSharedMin[0], gSharedMax[0]);
    }
}
)";

const std::string kGainQuantizeShader = R"(
StructuredBuffer<float4> gGainLog2Rgb : register(t0);
RWStructuredBuffer<uint> gGainMapPixels : register(u0);

cbuffer GainQuantizeConstants : register(b0) {
    uint gWidth;
    uint gHeight;
    float gMinGainLog2;
    float gMaxGainLog2;
    float gGamma;
    float gPadding0;
    float gPadding1;
    float gPadding2;
};

uint affine_map_gain(float gain_log2) {
    float mapped = clamp((gain_log2 - gMinGainLog2) / (gMaxGainLog2 - gMinGainLog2), 0.0f, 1.0f);
    if (gGamma != 1.0f) {
        mapped = pow(mapped, gGamma);
    }
    return (uint)clamp(floor(mapped * 255.0f + 0.5f), 0.0f, 255.0f);
}

uint pack_bgra8(uint b, uint g, uint r, uint a) {
    return (b & 0xFFu) | ((g & 0xFFu) << 8) | ((r & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (dispatchThreadId.x >= gWidth || dispatchThreadId.y >= gHeight) {
        return;
    }

    uint pixelIndex = dispatchThreadId.y * gWidth + dispatchThreadId.x;
    float3 gain = gGainLog2Rgb[pixelIndex].xyz;
    uint r = affine_map_gain(gain.x);
    uint g = affine_map_gain(gain.y);
    uint b = affine_map_gain(gain.z);
    gGainMapPixels[pixelIndex] = pack_bgra8(b, g, r, 255u);
}
)";

auto elapsed_ms(std::chrono::steady_clock::time_point start) -> long long {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               start)
      .count();
}

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
    return std::unexpected(std::format("Failed to create HDR constant buffer, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }
  return buffer;
}

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

auto create_structured_buffer(ID3D11Device* device, std::uint32_t element_count,
                              std::uint32_t element_size, bool need_uav, bool need_srv,
                              std::string_view name)
    -> std::expected<StructuredBuffer, std::string> {
  if (!device) {
    return std::unexpected("D3D device is null");
  }
  if (element_count == 0 || element_size == 0) {
    return std::unexpected(std::format("{} buffer size is empty", name));
  }

  auto byte_width_u64 = static_cast<std::uint64_t>(element_count) * element_size;
  if (byte_width_u64 > std::numeric_limits<UINT>::max()) {
    return std::unexpected(std::format("{} buffer is too large", name));
  }

  D3D11_BUFFER_DESC buffer_desc{};
  buffer_desc.ByteWidth = static_cast<UINT>(byte_width_u64);
  buffer_desc.Usage = D3D11_USAGE_DEFAULT;
  buffer_desc.BindFlags =
      (need_uav ? D3D11_BIND_UNORDERED_ACCESS : 0) | (need_srv ? D3D11_BIND_SHADER_RESOURCE : 0);
  buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  buffer_desc.StructureByteStride = element_size;

  StructuredBuffer result;
  result.element_count = element_count;
  result.element_size = element_size;

  HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, result.buffer.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create {} buffer, HRESULT: 0x{:08X}", name,
                                       static_cast<unsigned int>(hr)));
  }

  if (need_uav) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = element_count;

    hr = device->CreateUnorderedAccessView(result.buffer.get(), &uav_desc, result.uav.put());
    if (FAILED(hr)) {
      return std::unexpected(std::format("Failed to create {} UAV, HRESULT: 0x{:08X}", name,
                                         static_cast<unsigned int>(hr)));
    }
  }

  if (need_srv) {
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = element_count;

    hr = device->CreateShaderResourceView(result.buffer.get(), &srv_desc, result.srv.put());
    if (FAILED(hr)) {
      return std::unexpected(std::format("Failed to create {} SRV, HRESULT: 0x{:08X}", name,
                                         static_cast<unsigned int>(hr)));
    }
  }

  return result;
}

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

auto clear_compute_bindings(ID3D11DeviceContext* context) -> void {
  ID3D11ShaderResourceView* null_srvs[] = {nullptr, nullptr, nullptr};
  ID3D11UnorderedAccessView* null_uavs[] = {nullptr, nullptr, nullptr};
  ID3D11Buffer* null_cbs[] = {nullptr};
  context->CSSetShaderResources(0, 3, null_srvs);
  context->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, null_cbs);
  context->CSSetShader(nullptr, nullptr, 0);
}

auto dispatch_histogram(ID3D11Device* device, ID3D11DeviceContext* context,
                        ID3D11ShaderResourceView* source_srv, std::uint32_t width,
                        std::uint32_t height)
    -> std::expected<std::vector<std::uint32_t>, std::string> {
  auto shader_result = create_compute_shader(device, kHistogramComputeShader, "HDR histogram");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  auto histogram_buffer_result = create_structured_buffer(
      device, kHistogramBinCount, sizeof(std::uint32_t), true, false, "HDR histogram");
  if (!histogram_buffer_result) {
    return std::unexpected(histogram_buffer_result.error());
  }

  constexpr UINT kClearValues[4] = {0, 0, 0, 0};
  context->ClearUnorderedAccessViewUint(histogram_buffer_result->uav.get(), kClearValues);

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
  ID3D11UnorderedAccessView* uavs[] = {histogram_buffer_result->uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 1, srvs);
  context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch((width + kThreadGroupSizeX - 1) / kThreadGroupSizeX,
                    (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY, 1);
  clear_compute_bindings(context);

  return read_buffer_values<std::uint32_t>(device, context, histogram_buffer_result->buffer.get(),
                                           kHistogramBinCount);
}

auto dispatch_preprocess(ID3D11Device* device, ID3D11DeviceContext* context,
                         ID3D11ShaderResourceView* source_srv, std::uint32_t width,
                         std::uint32_t height, float content_peak_linear)
    -> std::expected<PreprocessOutputs, std::string> {
  auto shader_result = create_compute_shader(device, kPreprocessComputeShader, "HDR preprocess");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  const std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(width) * height;
  if (pixel_count_u64 == 0 || pixel_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("HDR image dimensions are invalid");
  }
  const auto pixel_count = static_cast<std::uint32_t>(pixel_count_u64);

  auto base_buffer_result =
      create_structured_buffer(device, pixel_count, kBaseBytesPerPixel, true, true, "HDR base");
  if (!base_buffer_result) {
    return std::unexpected(base_buffer_result.error());
  }

  auto hdr_buffer_result =
      create_structured_buffer(device, pixel_count, kHdrBytesPerPixel, true, true, "HDR half");
  if (!hdr_buffer_result) {
    return std::unexpected(hdr_buffer_result.error());
  }

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
  ID3D11UnorderedAccessView* uavs[] = {base_buffer_result->uav.get(), hdr_buffer_result->uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 1, srvs);
  context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch((width + kThreadGroupSizeX - 1) / kThreadGroupSizeX,
                    (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY, 1);
  clear_compute_bindings(context);

  return PreprocessOutputs{
      .base_bgra8_buffer = std::move(base_buffer_result.value()),
      .hdr_half_rgba_buffer = std::move(hdr_buffer_result.value()),
  };
}

auto dispatch_gain_compute(ID3D11Device* device, ID3D11DeviceContext* context,
                           const PreprocessOutputs& preprocess_outputs, std::uint32_t width,
                           std::uint32_t height) -> std::expected<GainComputeOutputs, std::string> {
  auto shader_result = create_compute_shader(device, kGainComputeShader, "HDR gain compute");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  const std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(width) * height;
  if (pixel_count_u64 == 0 || pixel_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("HDR image dimensions are invalid");
  }

  const std::uint32_t pixel_count = static_cast<std::uint32_t>(pixel_count_u64);
  const std::uint32_t groups_x = (width + kThreadGroupSizeX - 1) / kThreadGroupSizeX;
  const std::uint32_t groups_y = (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY;
  const std::uint64_t tile_count_u64 = static_cast<std::uint64_t>(groups_x) * groups_y;
  if (tile_count_u64 == 0 || tile_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("HDR tile dimensions are invalid");
  }
  const std::uint32_t tile_count = static_cast<std::uint32_t>(tile_count_u64);

  auto gain_buffer_result = create_structured_buffer(device, pixel_count, kGainLog2ElementBytes,
                                                     true, true, "HDR gain log2");
  if (!gain_buffer_result) {
    return std::unexpected(gain_buffer_result.error());
  }

  auto tile_minmax_result = create_structured_buffer(device, tile_count, kTileMinMaxElementBytes,
                                                     true, true, "HDR gain tile minmax");
  if (!tile_minmax_result) {
    return std::unexpected(tile_minmax_result.error());
  }

  GainComputeConstants constants{
      .width = width,
      .height = height,
      .groups_x = groups_x,
  };
  auto constant_buffer_result = create_constant_buffer(device, constants);
  if (!constant_buffer_result) {
    return std::unexpected(constant_buffer_result.error());
  }

  ID3D11ShaderResourceView* srvs[] = {preprocess_outputs.base_bgra8_buffer.srv.get(),
                                      preprocess_outputs.hdr_half_rgba_buffer.srv.get()};
  ID3D11UnorderedAccessView* uavs[] = {gain_buffer_result->uav.get(),
                                       tile_minmax_result->uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 2, srvs);
  context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch(groups_x, groups_y, 1);
  clear_compute_bindings(context);

  return GainComputeOutputs{
      .gain_log2_rgb_buffer = std::move(gain_buffer_result.value()),
      .tile_minmax_buffer = std::move(tile_minmax_result.value()),
  };
}

auto dispatch_reduce_tile_minmax(ID3D11Device* device, ID3D11DeviceContext* context,
                                 const StructuredBuffer& tile_minmax_buffer)
    -> std::expected<FloatMinMax, std::string> {
  auto reduce_shader =
      create_compute_shader(device, kReduceFloat2MinMaxComputeShader, "HDR gain tile reduce");
  if (!reduce_shader) {
    return std::unexpected(reduce_shader.error());
  }

  std::uint32_t current_count = tile_minmax_buffer.element_count;
  ID3D11ShaderResourceView* current_srv = tile_minmax_buffer.srv.get();
  StructuredBuffer intermediate_buffer;

  while (true) {
    const std::uint32_t group_count =
        (current_count + kReductionThreadCount - 1) / kReductionThreadCount;
    auto output_buffer_result = create_structured_buffer(
        device, group_count, kTileMinMaxElementBytes, true, true, "HDR gain tile reduce partial");
    if (!output_buffer_result) {
      return std::unexpected(output_buffer_result.error());
    }

    ReduceConstants constants{.element_count = current_count};
    auto constant_buffer_result = create_constant_buffer(device, constants);
    if (!constant_buffer_result) {
      return std::unexpected(constant_buffer_result.error());
    }

    ID3D11ShaderResourceView* srvs[] = {current_srv};
    ID3D11UnorderedAccessView* uavs[] = {output_buffer_result->uav.get()};
    ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
    context->CSSetShader(reduce_shader->get(), nullptr, 0);
    context->CSSetShaderResources(0, 1, srvs);
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    context->CSSetConstantBuffers(0, 1, cbs);
    context->Dispatch(group_count, 1, 1);
    clear_compute_bindings(context);

    if (group_count == 1) {
      auto minmax_values =
          read_buffer_values<FloatMinMax>(device, context, output_buffer_result->buffer.get(), 1);
      if (!minmax_values) {
        return std::unexpected(minmax_values.error());
      }
      return minmax_values->front();
    }

    intermediate_buffer = std::move(output_buffer_result.value());
    current_srv = intermediate_buffer.srv.get();
    current_count = group_count;
  }
}

auto finalize_gain_range(FloatMinMax reduced_range)
    -> std::expected<std::pair<float, float>, std::string> {
  float min_gain_log2 = reduced_range.min_value;
  float max_gain_log2 = reduced_range.max_value;
  if (!std::isfinite(min_gain_log2) || !std::isfinite(max_gain_log2)) {
    return std::unexpected("HDR gain range contains non-finite values");
  }

  min_gain_log2 = std::clamp(min_gain_log2, kGainMapMinClamp, kGainMapMaxClamp);
  max_gain_log2 = std::clamp(max_gain_log2, kGainMapMinClamp, kGainMapMaxClamp);
  if (std::fabs(max_gain_log2 - min_gain_log2) < std::numeric_limits<float>::epsilon()) {
    max_gain_log2 = std::min(max_gain_log2 + kGainMapMinRange, kGainMapMaxClamp);
  }
  if (max_gain_log2 <= min_gain_log2) {
    max_gain_log2 = min_gain_log2 + kGainMapMinRange;
  }

  return std::pair{min_gain_log2, max_gain_log2};
}

auto merge_gain_range_from_tiles(ID3D11Device* device, ID3D11DeviceContext* context,
                                 const GainComputeOutputs& gain_outputs)
    -> std::expected<std::pair<float, float>, std::string> {
  auto reduced_range =
      dispatch_reduce_tile_minmax(device, context, gain_outputs.tile_minmax_buffer);
  if (!reduced_range) {
    return std::unexpected(reduced_range.error());
  }
  return finalize_gain_range(reduced_range.value());
}

auto dispatch_gain_quantize(ID3D11Device* device, ID3D11DeviceContext* context,
                            const GainComputeOutputs& gain_outputs, std::uint32_t width,
                            std::uint32_t height, float min_gain_log2, float max_gain_log2)
    -> std::expected<StructuredBuffer, std::string> {
  auto shader_result = create_compute_shader(device, kGainQuantizeShader, "HDR gain quantize");
  if (!shader_result) {
    return std::unexpected(shader_result.error());
  }

  const std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(width) * height;
  if (pixel_count_u64 == 0 || pixel_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("HDR image dimensions are invalid");
  }
  const std::uint32_t pixel_count = static_cast<std::uint32_t>(pixel_count_u64);

  auto gainmap_result = create_structured_buffer(device, pixel_count, kGainMapBytesPerPixel, true,
                                                 false, "HDR gain map bgra8");
  if (!gainmap_result) {
    return std::unexpected(gainmap_result.error());
  }

  GainQuantizeConstants constants{
      .width = width,
      .height = height,
      .min_gain_log2 = min_gain_log2,
      .max_gain_log2 = max_gain_log2,
      .gamma = kGainMapGamma,
  };
  auto constant_buffer_result = create_constant_buffer(device, constants);
  if (!constant_buffer_result) {
    return std::unexpected(constant_buffer_result.error());
  }

  ID3D11ShaderResourceView* srvs[] = {gain_outputs.gain_log2_rgb_buffer.srv.get()};
  ID3D11UnorderedAccessView* uavs[] = {gainmap_result->uav.get()};
  ID3D11Buffer* cbs[] = {constant_buffer_result->get()};
  context->CSSetShader(shader_result->get(), nullptr, 0);
  context->CSSetShaderResources(0, 1, srvs);
  context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
  context->CSSetConstantBuffers(0, 1, cbs);
  context->Dispatch((width + kThreadGroupSizeX - 1) / kThreadGroupSizeX,
                    (height + kThreadGroupSizeY - 1) / kThreadGroupSizeY, 1);
  clear_compute_bindings(context);

  return gainmap_result;
}

}  // namespace GpuPreprocess

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

    auto histogram_start = std::chrono::steady_clock::now();
    auto histogram_result = GpuPreprocess::dispatch_histogram(
        device.get(), context.get(), source_srv_result->get(), desc.Width, desc.Height);
    if (!histogram_result) {
      return std::unexpected(histogram_result.error());
    }

    const auto pixel_count = static_cast<std::uint64_t>(desc.Width) * desc.Height;
    const float content_peak_linear =
        GpuPreprocess::compute_content_peak_linear(histogram_result.value(), pixel_count);
    const auto histogram_ms = GpuPreprocess::elapsed_ms(histogram_start);

    auto preprocess_start = std::chrono::steady_clock::now();
    auto preprocess_result =
        GpuPreprocess::dispatch_preprocess(device.get(), context.get(), source_srv_result->get(),
                                           desc.Width, desc.Height, content_peak_linear);
    if (!preprocess_result) {
      return std::unexpected(preprocess_result.error());
    }
    const auto preprocess_ms = GpuPreprocess::elapsed_ms(preprocess_start);

    auto gain_compute_start = std::chrono::steady_clock::now();
    auto gain_compute_result = GpuPreprocess::dispatch_gain_compute(
        device.get(), context.get(), preprocess_result.value(), desc.Width, desc.Height);
    if (!gain_compute_result) {
      return std::unexpected(gain_compute_result.error());
    }
    const auto gain_compute_ms = GpuPreprocess::elapsed_ms(gain_compute_start);

    auto gain_range_start = std::chrono::steady_clock::now();
    auto gain_range_result = GpuPreprocess::merge_gain_range_from_tiles(
        device.get(), context.get(), gain_compute_result.value());
    if (!gain_range_result) {
      return std::unexpected(gain_range_result.error());
    }
    const auto gain_range_ms = GpuPreprocess::elapsed_ms(gain_range_start);

    auto gain_quantize_start = std::chrono::steady_clock::now();
    auto gainmap_buffer_result = GpuPreprocess::dispatch_gain_quantize(
        device.get(), context.get(), gain_compute_result.value(), desc.Width, desc.Height,
        gain_range_result->first, gain_range_result->second);
    if (!gainmap_buffer_result) {
      return std::unexpected(gainmap_buffer_result.error());
    }
    const auto gain_quantize_ms = GpuPreprocess::elapsed_ms(gain_quantize_start);

    // gain 链路只依赖 GPU 上的 base/hdr buffer；base 读回放到 quantize 之后，避免挡住后续
    // dispatch。
    auto base_readback_start = std::chrono::steady_clock::now();
    auto base_bgra8_result = GpuPreprocess::read_buffer_bytes(
        device.get(), context.get(), preprocess_result->base_bgra8_buffer.buffer.get(),
        static_cast<std::size_t>(preprocess_result->base_bgra8_buffer.element_count) *
            GpuPreprocess::kBaseBytesPerPixel);
    if (!base_bgra8_result) {
      return std::unexpected(base_bgra8_result.error());
    }
    const auto base_readback_ms = GpuPreprocess::elapsed_ms(base_readback_start);

    auto gain_readback_start = std::chrono::steady_clock::now();
    auto gainmap_bgra8_result = GpuPreprocess::read_buffer_bytes(
        device.get(), context.get(), gainmap_buffer_result->buffer.get(),
        static_cast<std::size_t>(gainmap_buffer_result->element_count) *
            GpuPreprocess::kGainMapBytesPerPixel);
    if (!gainmap_bgra8_result) {
      return std::unexpected(gainmap_bgra8_result.error());
    }
    const auto gain_readback_ms = GpuPreprocess::elapsed_ms(gain_readback_start);

    Logger().debug(
        "HDR preprocess: histogram={} ms, preprocess={} ms, gain_compute={} ms, "
        "gain_range={} ms, gain_quantize={} ms, base_readback={} ms, gain_readback={} ms, "
        "total={} ms, content_peak_linear={:.4f}, gain_range=[{:.4f}, {:.4f}]",
        histogram_ms, preprocess_ms, gain_compute_ms, gain_range_ms, gain_quantize_ms,
        base_readback_ms, gain_readback_ms, GpuPreprocess::elapsed_ms(total_start),
        content_peak_linear, gain_range_result->first, gain_range_result->second);

    return UltraHdrPreparedImages{
        .base_bgra8 = std::move(base_bgra8_result.value()),
        .gainmap_bgra8 = std::move(gainmap_bgra8_result.value()),
        .min_gain_log2 = gain_range_result->first,
        .max_gain_log2 = gain_range_result->second,
        .width = desc.Width,
        .height = desc.Height,
    };
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("HDR GPU preprocess failed: {}", e.what()));
  }
}

}  // namespace Features::Screenshot::HdrEncoder
