module;

module Features.Screenshot.HdrEncoder;

import std;

namespace Features::Screenshot::HdrEncoder {

// ---- 本文件：HDR 浮点像素 → SDR 8-bit 底图 / libultrahdr 需要的 HDR half 平面 ----
// 捕获链给出的是 scRGB 类线性空间下的 RGB（relative scene-linear）。Ultra HDR 库要求 HDR
// 图层以「203 nit 为参考白」的线性量编码进 half；SDR 底图则是 tone-map 后 sRGB。
// 下面常量、矩阵与 PQ/ICtCp 运算都是为了：估计场景峰值、在感知域压高光、再落回 BT.709。

struct Float3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

// Windows HDR / scRGB 管线常以 80 nit 为参考白；libultrahdr 文档以 203 nit 为线性 half 的参考白。
// 两者比值用于把「捕获纹理里的线性 RGB」缩放到库期望的量纲。
constexpr float kScRgbReferenceWhiteNits = 80.0f;
constexpr float kUltraHdrReferenceWhiteNits = 203.0f;
constexpr float kScRgbToUltraHdrLinearScale =
    kScRgbReferenceWhiteNits / kUltraHdrReferenceWhiteNits;
constexpr float kMaxUltraHdrLinearValue = 10000.0f / kUltraHdrReferenceWhiteNits;
constexpr float kMaxPqLinearValue = 10000.0f / kScRgbReferenceWhiteNits;
// SDR 底图 tone-map 时假定「内容峰值」至少不会低于该线性量（约当场景不很亮时的默认肩线）。
constexpr float kSdrBasePeakLinearValue = 1.5f;
// 直方图取高分位估计场景峰值，避免单个噪点抬满峰值（0.9994 ≈ 保留最亮 0.06% 像素参与统计）。
constexpr float kContentPeakPercentile = 0.9994f;
constexpr std::size_t kHistogramBinCount = 4096;
// 半精度 1.0f 的位模式，用作 HDR 图层 Alpha（库侧通常期望不透明）。
constexpr std::uint16_t kHalfOne = 0x3C00;
constexpr std::uint8_t kOpaqueAlpha = 255;

// BT.709 相对线性 RGB ↔ XYZ ↔ LMS，再经 PQ 与非线性矩阵进出 ICtCp（ITU-R BT.2100 感知域）。
// 矩阵系数来自标准推荐值，用于在「亮度」与「色度」可分离的域里做 tone mapping。
constexpr std::array<float, 9> kRec709ToXyz = {
    0.4123907983f, 0.3575843275f, 0.1804807931f, 0.2126390040f, 0.7151686549f,
    0.0721923187f, 0.0193308182f, 0.1191947833f, 0.9505321383f,
};

constexpr std::array<float, 9> kXyzToRec709 = {
    3.2409698963f, -1.5373831987f, -0.4986107647f, -0.9692436457f, 1.8759675026f,
    0.0415550582f, 0.0556300804f,  -0.2039769590f, 1.0569715500f,
};

constexpr std::array<float, 9> kXyzToLms = {
    0.3592f, 0.6976f, -0.0358f, -0.1922f, 1.1004f, 0.0755f, 0.0070f, 0.0749f, 0.8434f,
};

constexpr std::array<float, 9> kLmsToXyz = {
    2.0701800567f,  -1.3264568761f, 0.2066160068f,  0.3649882500f, 0.6804673629f,
    -0.0454217531f, -0.0495955422f, -0.0494211612f, 1.1879959417f,
};

constexpr std::array<float, 9> kLmsPqToIctcp = {
    0.5000f, 0.5000f, 0.0000f, 1.6137f, -3.3234f, 1.7097f, 4.3780f, -4.2455f, -0.1325f,
};

constexpr std::array<float, 9> kIctcpToLmsPq = {
    1.0f,           0.0086051457f, 0.1110356045f, 1.0f,           -0.0086051457f,
    -0.1110356045f, 1.0f,          0.5600488596f, -0.3206374702f,
};

constexpr float kPqC1 = 107.0f / 128.0f;
constexpr float kPqC2 = 2413.0f / 128.0f;
constexpr float kPqC3 = 2392.0f / 128.0f;
constexpr float kPqM = 2523.0f / 32.0f;
constexpr float kPqN = 1305.0f / 8192.0f;
constexpr float kPqInvM = 1.0f / kPqM;
constexpr float kPqInvN = 1.0f / kPqN;

// 行主序 3×3 与列向量相乘。
auto mul_3x3(const std::array<float, 9>& matrix, const Float3& value) -> Float3 {
  return Float3{
      .x = matrix[0] * value.x + matrix[1] * value.y + matrix[2] * value.z,
      .y = matrix[3] * value.x + matrix[4] * value.y + matrix[5] * value.z,
      .z = matrix[6] * value.x + matrix[7] * value.y + matrix[8] * value.z,
  };
}

// IEEE754 binary16 ↔ binary32，自实现以免额外依赖；仅用于与 GPU R16 纹理数据交互。
auto half_to_float(std::uint16_t value) -> float {
  std::uint32_t sign = static_cast<std::uint32_t>(value & 0x8000) << 16;
  std::uint32_t exponent = (value >> 10) & 0x1F;
  std::uint32_t mantissa = value & 0x03FF;
  std::uint32_t bits = 0;

  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign;
    } else {
      int adjusted_exponent = -14;
      while ((mantissa & 0x0400) == 0) {
        mantissa <<= 1;
        --adjusted_exponent;
      }
      mantissa &= 0x03FF;
      bits = sign | (static_cast<std::uint32_t>(adjusted_exponent + 127) << 23) | (mantissa << 13);
    }
  } else if (exponent == 0x1F) {
    bits = sign | 0x7F800000u | (mantissa << 13);
  } else {
    bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
  }

  return std::bit_cast<float>(bits);
}

// 将 float 量化为 half；非正常值与过大值钳制到可写入 HDR 半精度层的有限范围。
auto float_to_half(float value) -> std::uint16_t {
  if (!std::isfinite(value)) {
    value = value > 0.0f ? kMaxUltraHdrLinearValue : 0.0f;
  }

  value = std::clamp(value, 0.0f, kMaxUltraHdrLinearValue);
  std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  std::uint32_t sign = (bits >> 16) & 0x8000u;
  int exponent = static_cast<int>((bits >> 23) & 0xFF) - 127 + 15;
  std::uint32_t mantissa = bits & 0x007FFFFFu;

  if (exponent <= 0) {
    if (exponent < -10) {
      return static_cast<std::uint16_t>(sign);
    }
    mantissa = (mantissa | 0x00800000u) >> (1 - exponent);
    return static_cast<std::uint16_t>(sign | ((mantissa + 0x00001000u) >> 13));
  }

  if (exponent >= 31) {
    return static_cast<std::uint16_t>(sign | 0x7C00u);
  }

  std::uint32_t rounded_mantissa = mantissa + 0x00001000u;
  if (rounded_mantissa & 0x00800000u) {
    rounded_mantissa = 0;
    ++exponent;
    if (exponent >= 31) {
      return static_cast<std::uint16_t>(sign | 0x7C00u);
    }
  }

  return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10) |
                                    (rounded_mantissa >> 13));
}

// NaN/Inf 视作无效样本，后续当黑色处理。
auto sanitize_linear(float value) -> float { return std::isfinite(value) ? value : 0.0f; }

// 将线性 0–1 编码为 sRGB 非线性值（仍为 0–1 float，再量化为 8-bit）。
auto srgb_oetf(float value) -> float {
  value = std::clamp(value, 0.0f, 1.0f);
  if (value <= 0.0031308f) {
    return value * 12.92f;
  }
  return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

auto float_to_u8(float value) -> std::uint8_t {
  value = std::clamp(value, 0.0f, 1.0f);
  return static_cast<std::uint8_t>(std::lround(value * 255.0f));
}

// PQ（ST-2084）：把绝对亮度归一到 [0,1] 的非线性码值；这里 max_linear_value 表示「场景白」缩放。
auto linear_to_pq(float value, float max_linear_value = kMaxPqLinearValue) -> float {
  if (!std::isfinite(value) || value == 0.0f) {
    return 0.0f;
  }

  float sign = value < 0.0f ? -1.0f : 1.0f;
  float normalized = std::clamp(std::abs(value) / max_linear_value, 0.0f, 1.0f);
  float powered = std::pow(normalized, kPqN);
  float encoded = std::pow((kPqC1 + kPqC2 * powered) / (1.0f + kPqC3 * powered), kPqM);
  return sign * encoded;
}

// PQ 反函数：把非线性码值还原到 scene-linear（仍乘在 max_linear_value 白点上）。
auto pq_to_linear(float value, float max_linear_value = kMaxPqLinearValue) -> float {
  if (!std::isfinite(value) || value == 0.0f) {
    return 0.0f;
  }

  float sign = value < 0.0f ? -1.0f : 1.0f;
  float encoded = std::pow(std::abs(value), kPqInvM);
  float denominator = kPqC2 - kPqC3 * encoded;
  if (denominator <= 0.0f) {
    return 0.0f;
  }

  float normalized = std::max(encoded - kPqC1, 0.0f) / denominator;
  return sign * std::pow(normalized, kPqInvN) * max_linear_value;
}

// BT.709 相对线性 RGB 的「亮度」近似（系数与变换到 XYZ 后再取 Y 一致）。
auto rec709_luminance(const Float3& rgb) -> float {
  return 0.2126390040f * rgb.x + 0.7151686549f * rgb.y + 0.0721923187f * rgb.z;
}

// scene-linear RGB → XYZ → LMS，对 LMS 逐分量施加 PQ，再线性组合为 ICtCp。
auto rec709_to_ictcp(const Float3& rgb) -> Float3 {
  Float3 xyz = mul_3x3(kRec709ToXyz, rgb);
  Float3 lms = mul_3x3(kXyzToLms, xyz);
  lms.x = linear_to_pq(std::max(lms.x, 0.0f));
  lms.y = linear_to_pq(std::max(lms.y, 0.0f));
  lms.z = linear_to_pq(std::max(lms.z, 0.0f));
  return mul_3x3(kLmsPqToIctcp, lms);
}

// ICtCp：I 接近感知亮度轴，Ct/Cp 携带色度；tone map 主要在 I 上动手，再缩放 Ct/Cp 抑制失真。
auto ictcp_to_rec709(const Float3& ictcp) -> Float3 {
  Float3 lms_pq = mul_3x3(kIctcpToLmsPq, ictcp);
  Float3 lms{
      .x = pq_to_linear(lms_pq.x),
      .y = pq_to_linear(lms_pq.y),
      .z = pq_to_linear(lms_pq.z),
  };
  Float3 xyz = mul_3x3(kLmsToXyz, lms);
  return mul_3x3(kXyzToRec709, xyz);
}

// 将 ICtCp 的 I 通道从 content_peak 压到 sdr_peak：简单有理式 + shoulder，避免硬切带来的条带。
auto tone_map_intensity(float input_intensity, float content_peak_pq, float sdr_peak_pq) -> float {
  if (input_intensity <= 0.0f || content_peak_pq <= sdr_peak_pq) {
    return input_intensity;
  }

  float shoulder_a = sdr_peak_pq / (content_peak_pq * content_peak_pq);
  float shoulder_b = 1.0f / sdr_peak_pq;
  float mapped = input_intensity * (1.0f + shoulder_a * input_intensity) /
                 (1.0f + shoulder_b * input_intensity);
  return std::clamp(mapped, 0.0f, 1.0f);
}

// 用亮度直方图估计「场景内容峰值」，供全局 tone-map 使用（比简单 max 更抗高光噪点）。
auto compute_content_peak_linear(const std::vector<std::uint16_t>& source_pixels) -> float {
  if (source_pixels.empty()) {
    return kSdrBasePeakLinearValue;
  }

  std::array<std::uint32_t, kHistogramBinCount> histogram{};
  std::uint64_t pixel_count = source_pixels.size() / 4;

  for (std::size_t i = 0; i < source_pixels.size(); i += 4) {
    Float3 rgb{
        .x = std::max(sanitize_linear(half_to_float(source_pixels[i + 0])), 0.0f),
        .y = std::max(sanitize_linear(half_to_float(source_pixels[i + 1])), 0.0f),
        .z = std::max(sanitize_linear(half_to_float(source_pixels[i + 2])), 0.0f),
    };
    float luminance = std::clamp(rec709_luminance(rgb), 0.0f, kMaxPqLinearValue);
    std::size_t bin = std::min<std::size_t>(
        static_cast<std::size_t>(luminance / kMaxPqLinearValue * kHistogramBinCount),
        kHistogramBinCount - 1);
    ++histogram[bin];
  }

  // 从最亮档向下累加，直到覆盖「最亮 1 - kContentPeakPercentile」的像素，把该档中点当作峰值估计。
  std::uint64_t highlight_pixel_count = std::max<std::uint64_t>(
      1,
      static_cast<std::uint64_t>(std::ceil(static_cast<double>(pixel_count) *
                                           (1.0 - static_cast<double>(kContentPeakPercentile)))));

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

// 生成 libultrahdr 所需的 8-bit sRGB SDR 底图：必要时在 ICtCp 域压高光，再 gamma 编码。
auto build_sdr_base_pixels(const std::vector<std::uint16_t>& source_pixels)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  constexpr std::size_t kChannelsPerPixel = 4;

  std::uint64_t pixel_count_u64 = source_pixels.size() / kChannelsPerPixel;
  if (pixel_count_u64 > std::numeric_limits<std::size_t>::max() / kChannelsPerPixel) {
    return std::unexpected("HDR image is too large");
  }

  std::size_t pixel_count = static_cast<std::size_t>(pixel_count_u64);
  std::vector<std::uint8_t> sdr_pixels(pixel_count * kChannelsPerPixel);

  float content_peak_linear = compute_content_peak_linear(source_pixels);
  float content_peak_pq = linear_to_pq(content_peak_linear);
  float sdr_peak_pq = linear_to_pq(kSdrBasePeakLinearValue);

  for (std::size_t i = 0; i < pixel_count; ++i) {
    std::size_t base = i * kChannelsPerPixel;
    Float3 rgb{
        .x = std::max(sanitize_linear(half_to_float(source_pixels[base + 0])), 0.0f),
        .y = std::max(sanitize_linear(half_to_float(source_pixels[base + 1])), 0.0f),
        .z = std::max(sanitize_linear(half_to_float(source_pixels[base + 2])), 0.0f),
    };

    Float3 sdr_linear = rgb;
    // 仅当场景峰值高于「SDR 参考峰」时才进 ICtCp tone-map；否则直接线性 → sRGB，避免无谓失真。
    if (content_peak_pq > sdr_peak_pq) {
      Float3 ictcp = rec709_to_ictcp(rgb);
      float input_intensity = std::max(ictcp.x, 0.0f);

      if (input_intensity > 0.0f) {
        float mapped_intensity = tone_map_intensity(input_intensity, content_peak_pq, sdr_peak_pq);
        // 亮度被压缩后，按比例收缩色度，减轻过饱和（简易 gamut 处理）。
        float chroma_scale = mapped_intensity > 0.0f
                                 ? std::clamp(std::min(input_intensity / mapped_intensity,
                                                       mapped_intensity / input_intensity),
                                              0.0f, 1.0f)
                                 : 0.0f;

        ictcp.x = mapped_intensity;
        ictcp.y *= chroma_scale;
        ictcp.z *= chroma_scale;
      } else {
        ictcp = {};
      }

      sdr_linear = ictcp_to_rec709(ictcp);
    }

    sdr_pixels[base + 0] = float_to_u8(srgb_oetf(std::max(sdr_linear.x, 0.0f)));
    sdr_pixels[base + 1] = float_to_u8(srgb_oetf(std::max(sdr_linear.y, 0.0f)));
    sdr_pixels[base + 2] = float_to_u8(srgb_oetf(std::max(sdr_linear.z, 0.0f)));
    sdr_pixels[base + 3] = kOpaqueAlpha;
  }

  return sdr_pixels;
}

// 供 libultrahdr 的 HDR 图层：在线性 BT.709 下把数值从「80 nit 参考」缩放到「203 nit 参考」，
// 再写入 half 纹理行；Alpha 固定为 1.0。
auto pack_ultrahdr_hdr_rgb_half_plane(const std::vector<std::uint16_t>& source_pixels)
    -> std::expected<std::vector<std::uint16_t>, std::string> {
  constexpr std::size_t kChannelsPerPixel = 4;

  std::uint64_t pixel_count_u64 = source_pixels.size() / kChannelsPerPixel;
  if (pixel_count_u64 > std::numeric_limits<std::size_t>::max() / kChannelsPerPixel) {
    return std::unexpected("HDR image is too large");
  }

  std::size_t pixel_count = static_cast<std::size_t>(pixel_count_u64);
  std::vector<std::uint16_t> hdr_pixels(pixel_count * kChannelsPerPixel);

  for (std::size_t i = 0; i < pixel_count; ++i) {
    std::size_t base = i * kChannelsPerPixel;
    float red = std::max(sanitize_linear(half_to_float(source_pixels[base + 0])), 0.0f);
    float green = std::max(sanitize_linear(half_to_float(source_pixels[base + 1])), 0.0f);
    float blue = std::max(sanitize_linear(half_to_float(source_pixels[base + 2])), 0.0f);

    hdr_pixels[base + 0] = float_to_half(red * kScRgbToUltraHdrLinearScale);
    hdr_pixels[base + 1] = float_to_half(green * kScRgbToUltraHdrLinearScale);
    hdr_pixels[base + 2] = float_to_half(blue * kScRgbToUltraHdrLinearScale);
    hdr_pixels[base + 3] = kHalfOne;
  }

  return hdr_pixels;
}

}  // namespace Features::Screenshot::HdrEncoder
