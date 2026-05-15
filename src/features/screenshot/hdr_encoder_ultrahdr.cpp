module;

module Features.Screenshot.HdrEncoder;

import std;
import Utils.Image;
import <windows.h>;

namespace Features::Screenshot::HdrEncoder {

namespace LocalUltraHdr {

constexpr std::uint8_t kJpegMarkerPrefix = 0xFF;
constexpr std::uint8_t kJpegMarkerSoi = 0xD8;
constexpr std::uint8_t kJpegMarkerSos = 0xDA;
constexpr std::uint8_t kJpegMarkerApp0 = 0xE0;
constexpr std::uint8_t kJpegMarkerApp1 = 0xE1;
constexpr std::uint8_t kJpegMarkerApp2 = 0xE2;

constexpr std::string_view kVersion = "1.0";
constexpr std::string_view kXmpNamespace = "http://ns.adobe.com/xap/1.0/";
constexpr std::string_view kIsoNamespace = "urn:iso:std:iso:ts:21496:-1";
constexpr std::string_view kContainerNamespace = "http://ns.google.com/photos/1.0/container/";
constexpr std::string_view kItemNamespace = "http://ns.google.com/photos/1.0/container/item/";
constexpr std::string_view kGainMapNamespace = "http://ns.adobe.com/hdr-gain-map/1.0/";

constexpr std::uint32_t kMpfPrimaryImageAttribute = 0x00030000;
constexpr std::uint32_t kMpfSecondaryImageAttribute = 0x00000000;
constexpr std::size_t kMpfAppSegmentHeaderBytes = 8;  // ff e2 + len + "MPF\0"

// 这份 ICC payload 来自 libultrahdr 生成的主图，用于表达当前这条链路固定的
// sRGB transfer + BT.709 gamut。这里直接内嵌最终 APP2 payload，避免再搬一整套 ICC writer。
constexpr std::array<std::uint8_t, 602> kPrimarySrgbIccPayload = {
    0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x02, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x04, 0x30, 0x00, 0x00, 0x6D, 0x6E, 0x74, 0x72, 0x52, 0x47,
    0x42, 0x20, 0x58, 0x59, 0x5A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0xF6, 0xD6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xD3, 0x2D, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x09, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x58, 0x72, 0x58,
    0x59, 0x5A, 0x00, 0x00, 0x01, 0x48, 0x00, 0x00, 0x00, 0x14, 0x67, 0x58, 0x59, 0x5A, 0x00, 0x00,
    0x01, 0x5C, 0x00, 0x00, 0x00, 0x14, 0x62, 0x58, 0x59, 0x5A, 0x00, 0x00, 0x01, 0x70, 0x00, 0x00,
    0x00, 0x14, 0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x01, 0x84, 0x00, 0x00, 0x00, 0x14, 0x72, 0x54,
    0x52, 0x43, 0x00, 0x00, 0x01, 0x98, 0x00, 0x00, 0x00, 0x28, 0x67, 0x54, 0x52, 0x43, 0x00, 0x00,
    0x01, 0xC0, 0x00, 0x00, 0x00, 0x28, 0x62, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xE8, 0x00, 0x00,
    0x00, 0x28, 0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x02, 0x10, 0x00, 0x00, 0x00, 0x3C, 0x6D, 0x6C,
    0x75, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0C, 0x65, 0x6E,
    0x55, 0x53, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x73, 0x00, 0x52, 0x00, 0x47,
    0x00, 0x42, 0x00, 0x20, 0x00, 0x47, 0x00, 0x61, 0x00, 0x6D, 0x00, 0x75, 0x00, 0x74, 0x00, 0x20,
    0x00, 0x77, 0x00, 0x69, 0x00, 0x74, 0x00, 0x68, 0x00, 0x20, 0x00, 0x73, 0x00, 0x52, 0x00, 0x47,
    0x00, 0x42, 0x00, 0x20, 0x00, 0x54, 0x00, 0x72, 0x00, 0x61, 0x00, 0x6E, 0x00, 0x73, 0x00, 0x66,
    0x00, 0x65, 0x00, 0x72, 0x00, 0x00, 0x58, 0x59, 0x5A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x6F, 0xA2, 0x00, 0x00, 0x38, 0xF5, 0x00, 0x00, 0x03, 0x90, 0x58, 0x59, 0x5A, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x62, 0x99, 0x00, 0x00, 0xB7, 0x85, 0x00, 0x00, 0x18, 0xDA, 0x58, 0x59,
    0x5A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0xA0, 0x00, 0x00, 0x0F, 0x84, 0x00, 0x00,
    0xB6, 0xCF, 0x58, 0x59, 0x5A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF6, 0xD6, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0xD3, 0x2D, 0x70, 0x61, 0x72, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x02, 0x66, 0x66, 0x00, 0x00, 0xF2, 0xA7, 0x00, 0x00, 0x0D, 0x59, 0x00, 0x00,
    0x13, 0xD0, 0x00, 0x00, 0x0A, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x61,
    0x72, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x66, 0x66, 0x00, 0x00,
    0xF2, 0xA7, 0x00, 0x00, 0x0D, 0x59, 0x00, 0x00, 0x13, 0xD0, 0x00, 0x00, 0x0A, 0x5B, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x61, 0x72, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x02, 0x66, 0x66, 0x00, 0x00, 0xF2, 0xA7, 0x00, 0x00, 0x0D, 0x59, 0x00, 0x00,
    0x13, 0xD0, 0x00, 0x00, 0x0A, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6D, 0x6C,
    0x75, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0C, 0x65, 0x6E,
    0x55, 0x53, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6F, 0x00, 0x6F,
    0x00, 0x67, 0x00, 0x6C, 0x00, 0x65, 0x00, 0x20, 0x00, 0x49, 0x00, 0x6E, 0x00, 0x63, 0x00, 0x2E,
    0x00, 0x20, 0x00, 0x32, 0x00, 0x30, 0x00, 0x32, 0x00, 0x32,
};

struct JpegPieces {
  // primary 只把开头那个 JFIF 段“提出来”前置，剩余部分原样保留。
  // 这样最终形状更接近 libultrahdr::appendGainMap()。
  std::vector<std::uint8_t> leading_jfif_segment;
  std::vector<std::uint8_t> tail_without_soi_and_jfif;
};

struct GainMapMetadataFractions {
  // ISO 21496-1 写入的是分数字段，不是 float。
  // 所以这里先把浮点语义模型转成“分子/分母”表示，再决定二进制布局。
  std::array<std::int32_t, 3> gain_map_min_n{};
  std::array<std::uint32_t, 3> gain_map_min_d{};
  std::array<std::int32_t, 3> gain_map_max_n{};
  std::array<std::uint32_t, 3> gain_map_max_d{};
  std::array<std::uint32_t, 3> gain_map_gamma_n{};
  std::array<std::uint32_t, 3> gain_map_gamma_d{};
  std::array<std::int32_t, 3> base_offset_n{};
  std::array<std::uint32_t, 3> base_offset_d{};
  std::array<std::int32_t, 3> alternate_offset_n{};
  std::array<std::uint32_t, 3> alternate_offset_d{};
  std::uint32_t base_hdr_headroom_n = 0;
  std::uint32_t base_hdr_headroom_d = 1;
  std::uint32_t alternate_hdr_headroom_n = 0;
  std::uint32_t alternate_hdr_headroom_d = 1;
  bool backward_direction = false;
  bool use_base_color_space = true;
};

auto app_segment_size(std::size_t payload_size) -> std::size_t { return 4 + payload_size; }

auto append_u8(std::vector<std::uint8_t>& out, std::uint8_t value) -> void { out.push_back(value); }

auto append_u16_be(std::vector<std::uint8_t>& out, std::uint16_t value) -> void {
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

auto append_u32_be(std::vector<std::uint8_t>& out, std::uint32_t value) -> void {
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

auto append_s32_be(std::vector<std::uint8_t>& out, std::int32_t value) -> void {
  append_u32_be(out, static_cast<std::uint32_t>(value));
}

auto append_bytes(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> bytes) -> void {
  out.insert(out.end(), bytes.begin(), bytes.end());
}

auto append_string_bytes(std::vector<std::uint8_t>& out, std::string_view value) -> void {
  out.insert(out.end(), value.begin(), value.end());
}

auto format_xmp_float(float value) -> std::string {
  // XMP 是文本字段，这里统一限制成稳定的短浮点格式，避免无意义的小数抖动。
  return std::format("{:.9g}", value);
}

auto is_jfif_payload(std::span<const std::uint8_t> payload) -> bool {
  return payload.size() >= 5 && payload[0] == 'J' && payload[1] == 'F' && payload[2] == 'I' &&
         payload[3] == 'F' && payload[4] == 0;
}

auto split_wic_jpeg(std::span<const std::uint8_t> jpeg) -> std::expected<JpegPieces, std::string> {
  // primary JPEG 的策略是：
  // - SOI 自己重写
  // - 如果 WIC 产出了 JFIF，就把这一个 APP0 单独提出来
  // - 其余编码器产出的真正图像 tail 原样保留
  if (jpeg.size() < 4 || jpeg[0] != kJpegMarkerPrefix || jpeg[1] != kJpegMarkerSoi) {
    return std::unexpected("Invalid JPEG SOI");
  }

  std::size_t tail_start = 2;
  bool tail_started = false;
  JpegPieces pieces;

  for (std::size_t i = 2; i + 1 < jpeg.size();) {
    if (jpeg[i] != kJpegMarkerPrefix) {
      return std::unexpected("Invalid JPEG marker alignment before SOS");
    }

    const std::size_t segment_start = i;
    while (i < jpeg.size() && jpeg[i] == kJpegMarkerPrefix) {
      ++i;
    }
    if (i >= jpeg.size()) {
      return std::unexpected("JPEG marker truncated");
    }

    const std::uint8_t marker = jpeg[i++];
    if (marker == kJpegMarkerSos) {
      if (!tail_started) {
        tail_start = segment_start;
      }
      pieces.tail_without_soi_and_jfif.assign(
          jpeg.begin() + static_cast<std::ptrdiff_t>(tail_start), jpeg.end());
      return pieces;
    }

    if (i + 1 >= jpeg.size()) {
      return std::unexpected("JPEG segment header truncated");
    }

    const std::size_t segment_length =
        (static_cast<std::size_t>(jpeg[i]) << 8) | static_cast<std::size_t>(jpeg[i + 1]);
    if (segment_length < 2 || i + segment_length > jpeg.size()) {
      return std::unexpected("Invalid JPEG segment length");
    }

    const std::size_t total_segment_bytes = 2 + segment_length;
    std::span<const std::uint8_t> payload(jpeg.data() + i + 2, segment_length - 2);
    const bool is_app_segment = marker >= 0xE0 && marker <= 0xEF;

    if (!tail_started) {
      if (marker == kJpegMarkerApp0 && pieces.leading_jfif_segment.empty() &&
          is_jfif_payload(payload)) {
        pieces.leading_jfif_segment.assign(
            jpeg.begin() + static_cast<std::ptrdiff_t>(segment_start),
            jpeg.begin() + static_cast<std::ptrdiff_t>(segment_start + total_segment_bytes));
        tail_start = segment_start + total_segment_bytes;
      } else if (is_app_segment) {
        return std::unexpected("Unexpected APP segment in WIC JPEG before image tables");
      } else {
        tail_started = true;
        tail_start = segment_start;
      }
    } else if (is_app_segment) {
      return std::unexpected("Unexpected APP segment after JPEG image tables began");
    }

    i += segment_length;
  }

  return std::unexpected("JPEG SOS marker not found");
}

auto strip_jpeg_soi(std::span<const std::uint8_t> jpeg)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  // secondary 更简单：和 libultrahdr 一样，只剥掉 SOI，剩余 tail 全部原样拼回去。
  if (jpeg.size() < 2 || jpeg[0] != kJpegMarkerPrefix || jpeg[1] != kJpegMarkerSoi) {
    return std::unexpected("Invalid JPEG SOI");
  }

  return std::vector<std::uint8_t>(jpeg.begin() + 2, jpeg.end());
}

auto append_app_segment(std::vector<std::uint8_t>& out, std::uint8_t marker,
                        std::span<const std::uint8_t> payload) -> std::expected<void, std::string> {
  // JPEG APP 段长度字段包含“长度字段自身的 2 字节”，但不包含前面的 marker。
  const std::size_t length_field_value = 2 + payload.size();
  if (length_field_value > std::numeric_limits<std::uint16_t>::max()) {
    return std::unexpected("JPEG APP segment is too large");
  }

  append_u8(out, kJpegMarkerPrefix);
  append_u8(out, marker);
  append_u16_be(out, static_cast<std::uint16_t>(length_field_value));
  append_bytes(out, payload);
  return {};
}

auto make_namespaced_payload(std::string_view ns, std::span<const std::uint8_t> body)
    -> std::vector<std::uint8_t> {
  // XMP / ISO APP 段都需要 namespace\0 + body 的形状。
  std::vector<std::uint8_t> payload;
  payload.reserve(ns.size() + 1 + body.size());
  append_string_bytes(payload, ns);
  append_u8(payload, 0);
  append_bytes(payload, body);
  return payload;
}

auto float_to_unsigned_fraction_impl(float value, std::uint32_t max_numerator,
                                     std::uint32_t* numerator, std::uint32_t* denominator) -> bool {
  // 这里用 continued fraction 逼近 float，逻辑来自“把实数稳定写成有理数”的常见做法。
  // 目标不是数学上绝对最优，而是得到一个可编码且足够精确的 N/D。
  if (!std::isfinite(value) || value < 0.0f || value > max_numerator) {
    return false;
  }

  const std::uint64_t max_denominator =
      value <= 1.0f ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint64_t>(std::floor(max_numerator / value));

  *denominator = 1;
  std::uint32_t previous_denominator = 0;
  double current_value = static_cast<double>(value) - std::floor(value);
  constexpr int kMaxIterations = 39;

  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    const double numerator_double = static_cast<double>(*denominator) * value;
    if (numerator_double > max_numerator) {
      return false;
    }

    *numerator = static_cast<std::uint32_t>(std::round(numerator_double));
    if (std::fabs(numerator_double - *numerator) == 0.0) {
      return true;
    }

    current_value = 1.0 / current_value;
    const double new_denominator =
        previous_denominator + std::floor(current_value) * (*denominator);
    if (new_denominator > max_denominator) {
      return true;
    }

    previous_denominator = *denominator;
    if (new_denominator > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
      return false;
    }

    *denominator = static_cast<std::uint32_t>(new_denominator);
    current_value -= std::floor(current_value);
  }

  *numerator = static_cast<std::uint32_t>(std::round(static_cast<double>(*denominator) * value));
  return true;
}

auto float_to_unsigned_fraction(float value, std::uint32_t* numerator, std::uint32_t* denominator)
    -> bool {
  return float_to_unsigned_fraction_impl(value, std::numeric_limits<std::uint32_t>::max(),
                                         numerator, denominator);
}

auto float_to_signed_fraction(float value, std::int32_t* numerator, std::uint32_t* denominator)
    -> bool {
  std::uint32_t positive_numerator = 0;
  if (!float_to_unsigned_fraction_impl(std::fabs(value), std::numeric_limits<std::int32_t>::max(),
                                       &positive_numerator, denominator)) {
    return false;
  }

  *numerator = static_cast<std::int32_t>(positive_numerator);
  if (value < 0.0f) {
    *numerator *= -1;
  }
  return true;
}

auto convert_metadata_to_fractions(const GainMapMetadata& metadata)
    -> std::expected<GainMapMetadataFractions, std::string> {
  // 浮点语义模型 -> ISO 分数字段。
  // boost / hdr_capacity 在文件里实际写的是 log2 域，所以这里先转 log2。
  GainMapMetadataFractions result;
  result.backward_direction = false;
  result.use_base_color_space = metadata.use_base_cg;

  const float log2_min_boost = std::log2(metadata.min_content_boost);
  const float log2_max_boost = std::log2(metadata.max_content_boost);
  const float log2_hdr_capacity_min = std::log2(metadata.hdr_capacity_min);
  const float log2_hdr_capacity_max = std::log2(metadata.hdr_capacity_max);

  if (!float_to_signed_fraction(log2_min_boost, &result.gain_map_min_n[0],
                                &result.gain_map_min_d[0])) {
    return std::unexpected("Failed to encode gain map min boost as fraction");
  }
  if (!float_to_signed_fraction(log2_max_boost, &result.gain_map_max_n[0],
                                &result.gain_map_max_d[0])) {
    return std::unexpected("Failed to encode gain map max boost as fraction");
  }
  if (!float_to_unsigned_fraction(metadata.gamma, &result.gain_map_gamma_n[0],
                                  &result.gain_map_gamma_d[0])) {
    return std::unexpected("Failed to encode gain map gamma as fraction");
  }
  if (!float_to_signed_fraction(metadata.offset_sdr, &result.base_offset_n[0],
                                &result.base_offset_d[0])) {
    return std::unexpected("Failed to encode SDR offset as fraction");
  }
  if (!float_to_signed_fraction(metadata.offset_hdr, &result.alternate_offset_n[0],
                                &result.alternate_offset_d[0])) {
    return std::unexpected("Failed to encode HDR offset as fraction");
  }
  for (int i = 1; i < 3; ++i) {
    result.gain_map_min_n[i] = result.gain_map_min_n[0];
    result.gain_map_min_d[i] = result.gain_map_min_d[0];
    result.gain_map_max_n[i] = result.gain_map_max_n[0];
    result.gain_map_max_d[i] = result.gain_map_max_d[0];
    result.gain_map_gamma_n[i] = result.gain_map_gamma_n[0];
    result.gain_map_gamma_d[i] = result.gain_map_gamma_d[0];
    result.base_offset_n[i] = result.base_offset_n[0];
    result.base_offset_d[i] = result.base_offset_d[0];
    result.alternate_offset_n[i] = result.alternate_offset_n[0];
    result.alternate_offset_d[i] = result.alternate_offset_d[0];
  }

  if (!float_to_unsigned_fraction(log2_hdr_capacity_min, &result.base_hdr_headroom_n,
                                  &result.base_hdr_headroom_d)) {
    return std::unexpected("Failed to encode HDR capacity min as fraction");
  }
  if (!float_to_unsigned_fraction(log2_hdr_capacity_max, &result.alternate_hdr_headroom_n,
                                  &result.alternate_hdr_headroom_d)) {
    return std::unexpected("Failed to encode HDR capacity max as fraction");
  }

  return result;
}

auto should_use_common_denominator(const GainMapMetadataFractions& metadata) -> bool {
  // 只有所有字段分母真的相同，才能写 compact/common-denominator 布局。
  // 这里必须数据驱动判断，不能拍脑袋固定某个 flag。
  const std::uint32_t denominator = metadata.base_hdr_headroom_d;
  if (metadata.alternate_hdr_headroom_d != denominator) {
    return false;
  }

  return metadata.gain_map_min_d[0] == denominator && metadata.gain_map_max_d[0] == denominator &&
         metadata.gain_map_gamma_d[0] == denominator && metadata.base_offset_d[0] == denominator &&
         metadata.alternate_offset_d[0] == denominator;
}

auto serialize_iso21496_1_from_fractions(const GainMapMetadataFractions& metadata)
    -> std::vector<std::uint8_t> {
  // 这一步才真正把分数字段落成 ISO 21496-1 二进制。
  // 当前产品语义已经固定成“多通道 gain map + 共享一套 metadata 参数”。
  // 所以这里直接写单套参数布局，不再保留历史上的 1/3 通道动态判断形状。
  constexpr std::uint8_t channel_count = 1;
  const bool use_common_denominator = should_use_common_denominator(metadata);

  std::vector<std::uint8_t> out;
  out.reserve(use_common_denominator ? 64 : 96);

  append_u16_be(out, 0);
  append_u16_be(out, 0);

  std::uint8_t flags = 0;
  if (channel_count == 3) {
    flags |= (1u << 7);
  }
  if (metadata.use_base_color_space) {
    flags |= (1u << 6);
  }
  if (metadata.backward_direction) {
    flags |= 4;
  }
  if (use_common_denominator) {
    flags |= (1u << 3);
  }
  append_u8(out, flags);

  if (use_common_denominator) {
    // common denominator 布局更短，但前提比看起来严格得多。
    const std::uint32_t common_denominator = metadata.base_hdr_headroom_d;
    append_u32_be(out, common_denominator);
    append_u32_be(out, metadata.base_hdr_headroom_n);
    append_u32_be(out, metadata.alternate_hdr_headroom_n);

    for (std::uint8_t channel = 0; channel < channel_count; ++channel) {
      append_s32_be(out, metadata.gain_map_min_n[channel]);
      append_s32_be(out, metadata.gain_map_max_n[channel]);
      append_u32_be(out, metadata.gain_map_gamma_n[channel]);
      append_s32_be(out, metadata.base_offset_n[channel]);
      append_s32_be(out, metadata.alternate_offset_n[channel]);
    }
  } else {
    // 非 common denominator 是更通用的布局，也是我们当前实拍文件实际走的路径。
    append_u32_be(out, metadata.base_hdr_headroom_n);
    append_u32_be(out, metadata.base_hdr_headroom_d);
    append_u32_be(out, metadata.alternate_hdr_headroom_n);
    append_u32_be(out, metadata.alternate_hdr_headroom_d);

    for (std::uint8_t channel = 0; channel < channel_count; ++channel) {
      append_s32_be(out, metadata.gain_map_min_n[channel]);
      append_u32_be(out, metadata.gain_map_min_d[channel]);
      append_s32_be(out, metadata.gain_map_max_n[channel]);
      append_u32_be(out, metadata.gain_map_max_d[channel]);
      append_u32_be(out, metadata.gain_map_gamma_n[channel]);
      append_u32_be(out, metadata.gain_map_gamma_d[channel]);
      append_s32_be(out, metadata.base_offset_n[channel]);
      append_u32_be(out, metadata.base_offset_d[channel]);
      append_s32_be(out, metadata.alternate_offset_n[channel]);
      append_u32_be(out, metadata.alternate_offset_d[channel]);
    }
  }

  return out;
}

auto encode_gainmap_metadata(const GainMapMetadata& metadata)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  auto fraction_metadata_result = convert_metadata_to_fractions(metadata);
  if (!fraction_metadata_result) {
    return std::unexpected(fraction_metadata_result.error());
  }

  return serialize_iso21496_1_from_fractions(fraction_metadata_result.value());
}

auto generate_xmp_for_primary_image(std::size_t secondary_image_length) -> std::string {
  // primary XMP 主要描述“这个 JPEG 容器里还有一张 gain map JPEG，长度是多少”。
  return std::format(
      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Adobe XMP Core 5.1.2\">"
      "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
      "<rdf:Description xmlns:Container=\"{}\" xmlns:Item=\"{}\" xmlns:hdrgm=\"{}\" "
      "hdrgm:Version=\"{}\">"
      "<Container:Directory><rdf:Seq>"
      "<rdf:li rdf:parseType=\"Resource\"><Container:Item Item:Semantic=\"Primary\" "
      "Item:Mime=\"image/jpeg\"/></rdf:li>"
      "<rdf:li rdf:parseType=\"Resource\"><Container:Item Item:Semantic=\"GainMap\" "
      "Item:Mime=\"image/jpeg\" Item:Length=\"{}\"/></rdf:li>"
      "</rdf:Seq></Container:Directory>"
      "</rdf:Description></rdf:RDF></x:xmpmeta>",
      kContainerNamespace, kItemNamespace, kGainMapNamespace, kVersion, secondary_image_length);
}

auto generate_xmp_for_secondary_image(const GainMapMetadata& metadata) -> std::string {
  // secondary XMP 写的是 gain map 的浮点语义，便于支持 XMP 的解码端直接读取。
  return std::format(
      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Adobe XMP Core 5.1.2\">"
      "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
      "<rdf:Description xmlns:hdrgm=\"{}\" hdrgm:Version=\"{}\" hdrgm:GainMapMin=\"{}\" "
      "hdrgm:GainMapMax=\"{}\" hdrgm:Gamma=\"{}\" hdrgm:OffsetSDR=\"{}\" "
      "hdrgm:OffsetHDR=\"{}\" hdrgm:HDRCapacityMin=\"{}\" hdrgm:HDRCapacityMax=\"{}\" "
      "hdrgm:BaseRenditionIsHDR=\"False\"/>"
      "</rdf:RDF></x:xmpmeta>",
      kGainMapNamespace, kVersion, format_xmp_float(std::log2(metadata.min_content_boost)),
      format_xmp_float(std::log2(metadata.max_content_boost)), format_xmp_float(metadata.gamma),
      format_xmp_float(metadata.offset_sdr), format_xmp_float(metadata.offset_hdr),
      format_xmp_float(std::log2(metadata.hdr_capacity_min)),
      format_xmp_float(std::log2(metadata.hdr_capacity_max)));
}

auto generate_mpf_payload(std::uint32_t primary_image_size, std::uint32_t secondary_image_size,
                          std::uint32_t secondary_image_offset) -> std::vector<std::uint8_t> {
  // MPF 是“多图片 JPEG”的目录。这里只写最小二图目录：
  // - image0: primary
  // - image1: gain map
  std::vector<std::uint8_t> payload;
  payload.reserve(86);

  append_string_bytes(payload, "MPF");
  append_u8(payload, 0);
  append_u8(payload, 'M');
  append_u8(payload, 'M');
  append_u8(payload, 0);
  append_u8(payload, 0x2A);
  append_u32_be(payload, 8);
  append_u16_be(payload, 3);

  append_u16_be(payload, 0xB000);
  append_u16_be(payload, 0x0007);
  append_u32_be(payload, 4);
  append_string_bytes(payload, "0100");

  append_u16_be(payload, 0xB001);
  append_u16_be(payload, 0x0004);
  append_u32_be(payload, 1);
  append_u32_be(payload, 2);

  append_u16_be(payload, 0xB002);
  append_u16_be(payload, 0x0007);
  append_u32_be(payload, 32);
  append_u32_be(payload, 50);

  append_u32_be(payload, 0);

  append_u32_be(payload, kMpfPrimaryImageAttribute);
  append_u32_be(payload, primary_image_size);
  append_u32_be(payload, 0);
  append_u16_be(payload, 0);
  append_u16_be(payload, 0);

  append_u32_be(payload, kMpfSecondaryImageAttribute);
  append_u32_be(payload, secondary_image_size);
  append_u32_be(payload, secondary_image_offset);
  append_u16_be(payload, 0);
  append_u16_be(payload, 0);

  return payload;
}

auto assemble_ultrahdr_jpeg(std::span<const std::uint8_t> base_jpeg,
                            std::span<const std::uint8_t> gainmap_jpeg,
                            const GainMapMetadata& metadata)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  // 这就是最终 mux 层：
  // 1) base JPEG / gain map JPEG 先分别由 WIC 产出
  // 2) 这里再补 XMP / ISO / ICC / MPF，拼成单文件 Ultra HDR JPEG
  auto base_pieces_result = split_wic_jpeg(base_jpeg);
  if (!base_pieces_result) {
    return std::unexpected("Base JPEG parse failed: " + base_pieces_result.error());
  }

  auto gainmap_tail_result = strip_jpeg_soi(gainmap_jpeg);
  if (!gainmap_tail_result) {
    return std::unexpected("Gain map JPEG parse failed: " + gainmap_tail_result.error());
  }

  const auto& base_pieces = base_pieces_result.value();
  const auto& gainmap_tail_without_soi = gainmap_tail_result.value();

  auto iso_secondary_body_result = encode_gainmap_metadata(metadata);
  if (!iso_secondary_body_result) {
    return std::unexpected(iso_secondary_body_result.error());
  }

  const auto xmp_secondary = generate_xmp_for_secondary_image(metadata);
  const auto xmp_secondary_payload = make_namespaced_payload(
      kXmpNamespace,
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(xmp_secondary.data()),
                                    xmp_secondary.size()));
  const auto iso_secondary_payload =
      make_namespaced_payload(kIsoNamespace, iso_secondary_body_result.value());

  const std::size_t secondary_image_size = 2 + app_segment_size(xmp_secondary_payload.size()) +
                                           app_segment_size(iso_secondary_payload.size()) +
                                           gainmap_tail_without_soi.size();

  const auto xmp_primary = generate_xmp_for_primary_image(secondary_image_size);
  const auto xmp_primary_payload = make_namespaced_payload(
      kXmpNamespace,
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(xmp_primary.data()),
                                    xmp_primary.size()));
  const std::array<std::uint8_t, 4> iso_primary_body = {0, 0, 0, 0};
  const auto iso_primary_payload = make_namespaced_payload(kIsoNamespace, iso_primary_body);

  std::vector<std::uint8_t> out;
  out.reserve(base_jpeg.size() + gainmap_jpeg.size() + 512);

  append_u8(out, kJpegMarkerPrefix);
  append_u8(out, kJpegMarkerSoi);
  // primary 开头保留原 JFIF。很多查看器虽不强制要求，但这是最接近参考实现的形状。
  append_bytes(out, base_pieces.leading_jfif_segment);

  if (auto result = append_app_segment(out, kJpegMarkerApp1, xmp_primary_payload); !result) {
    return std::unexpected(result.error());
  }
  if (auto result = append_app_segment(out, kJpegMarkerApp2,
                                       std::span<const std::uint8_t>(kPrimarySrgbIccPayload));
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result = append_app_segment(out, kJpegMarkerApp2, iso_primary_payload); !result) {
    return std::unexpected(result.error());
  }

  // MPF 的 primary size / secondary offset 依赖“最终写出的字节位置”，
  // 所以这里先用占位 payload 估尺寸，再生成真正的 payload。
  auto mpf_payload = generate_mpf_payload(0, static_cast<std::uint32_t>(secondary_image_size), 0);
  // primary tail 这里仍需把原 JPEG 的 SOI 计入，才能与 libultrahdr 的 primary_image_size 一致。
  const std::size_t primary_image_size =
      out.size() + (2 + mpf_payload.size()) + 2 + base_pieces.tail_without_soi_and_jfif.size();

  if (primary_image_size > std::numeric_limits<std::uint32_t>::max() ||
      secondary_image_size > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("Ultra HDR JPEG is too large for MPF offsets");
  }

  const std::size_t mpf_data_start_offset = out.size() + kMpfAppSegmentHeaderBytes;
  if (primary_image_size < mpf_data_start_offset) {
    return std::unexpected("Invalid MPF offset calculation");
  }

  const std::uint32_t secondary_image_offset =
      static_cast<std::uint32_t>(primary_image_size - mpf_data_start_offset);
  mpf_payload = generate_mpf_payload(static_cast<std::uint32_t>(primary_image_size),
                                     static_cast<std::uint32_t>(secondary_image_size),
                                     secondary_image_offset);
  if (auto result = append_app_segment(out, kJpegMarkerApp2, mpf_payload); !result) {
    return std::unexpected(result.error());
  }

  append_bytes(out, base_pieces.tail_without_soi_and_jfif);

  append_u8(out, kJpegMarkerPrefix);
  append_u8(out, kJpegMarkerSoi);
  if (auto result = append_app_segment(out, kJpegMarkerApp1, xmp_secondary_payload); !result) {
    return std::unexpected(result.error());
  }
  if (auto result = append_app_segment(out, kJpegMarkerApp2, iso_secondary_payload); !result) {
    return std::unexpected(result.error());
  }
  append_bytes(out, gainmap_tail_without_soi);

  return out;
}

}  // namespace LocalUltraHdr

auto build_gainmap_metadata(const UltraHdrPreparedImages& images,
                            const UltraHdrEncodeOptions& options) -> GainMapMetadata {
  // 这里把 GPU 统计出的 gain 范围翻回文件元数据语义。
  // 当前策略固定为：
  // - gamma = 1
  // - offset = 1e-7
  // - hdr_capacity_min = 1
  // - hdr_capacity_max = target_display_peak / 203
  const float peak_nits = std::clamp(options.target_display_peak_nits, 203.0f, 10000.0f);

  GainMapMetadata metadata;
  metadata.min_content_boost = std::exp2(images.min_gain_log2);
  metadata.max_content_boost = std::exp2(images.max_gain_log2);
  metadata.gamma = 1.0f;
  metadata.offset_sdr = 1e-7f;
  metadata.offset_hdr = 1e-7f;
  metadata.hdr_capacity_min = 1.0f;
  metadata.hdr_capacity_max = peak_nits / 203.0f;
  metadata.use_base_cg = true;
  return metadata;
}

auto encode_bgra_layer_to_jpeg(const std::vector<std::uint8_t>& pixels, std::uint32_t width,
                               std::uint32_t height, float jpeg_quality)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  auto factory_result = Utils::Image::get_thread_wic_factory();
  if (!factory_result) {
    return std::unexpected(factory_result.error());
  }
  return Utils::Image::encode_bgra_to_jpeg_bytes(factory_result->get(), pixels.data(), width,
                                                 height, width * 4, jpeg_quality);
}

auto encode_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                          const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  constexpr std::size_t kBaseBytesPerPixel = 4;
  constexpr std::size_t kGainMapBytesPerPixel = 4;
  const std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(images.width) * images.height;
  if (pixel_count_u64 == 0) {
    return std::unexpected("Ultra HDR prepared image dimensions are empty");
  }

  if (pixel_count_u64 > std::numeric_limits<std::size_t>::max() / kBaseBytesPerPixel) {
    return std::unexpected("Ultra HDR image is too large");
  }

  const std::size_t expected_base_bytes =
      static_cast<std::size_t>(pixel_count_u64) * kBaseBytesPerPixel;
  const std::size_t expected_gainmap_bytes =
      static_cast<std::size_t>(pixel_count_u64) * kGainMapBytesPerPixel;
  if (images.base_bgra8.size() != expected_base_bytes ||
      images.gainmap_bgra8.size() != expected_gainmap_bytes) {
    return std::unexpected("Ultra HDR prepared image buffer size mismatch");
  }

  // WIC 接口收的是 0..1，而外部选项更适合暴露成 0..100。
  const float base_quality = std::clamp(options.base_quality / 100.0f, 0.0f, 1.0f);
  const float gainmap_quality = std::clamp(options.gainmap_quality / 100.0f, 0.0f, 1.0f);
  const auto metadata = build_gainmap_metadata(images, options);

  const std::uint32_t width = images.width;
  const std::uint32_t height = images.height;

  // base / gainmap 互不依赖，各在线程用 thread_local WIC 工厂并行编码。
  auto base_future = std::async(std::launch::async, [&images, width, height, base_quality]() {
    return encode_bgra_layer_to_jpeg(images.base_bgra8, width, height, base_quality);
  });
  auto gainmap_future = std::async(std::launch::async, [&images, width, height, gainmap_quality]() {
    return encode_bgra_layer_to_jpeg(images.gainmap_bgra8, width, height, gainmap_quality);
  });

  auto base_jpeg = base_future.get();
  auto gainmap_jpeg = gainmap_future.get();
  if (!base_jpeg) {
    return std::unexpected("Base JPEG encode failed: " + base_jpeg.error());
  }
  if (!gainmap_jpeg) {
    return std::unexpected("Gain map JPEG encode failed: " + gainmap_jpeg.error());
  }

  return LocalUltraHdr::assemble_ultrahdr_jpeg(base_jpeg.value(), gainmap_jpeg.value(), metadata);
}

auto write_file(const std::wstring& file_path, const std::vector<std::uint8_t>& data)
    -> std::expected<void, std::string> {
  // 最终输出就是普通 .jpg 文件；Ultra HDR 的所有额外语义都已经在二进制段里。
  std::ofstream file(std::filesystem::path(file_path), std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open Ultra HDR output file");
  }

  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file) {
    return std::unexpected("Failed to write Ultra HDR output file");
  }

  return {};
}

}  // namespace Features::Screenshot::HdrEncoder
