module;

#include <ultrahdr_api.h>
#include <windows.h>

module Features.Screenshot.HdrEncoder;

import std;
import Utils.Image.UhdrJpegRemux;

namespace Features::Screenshot::HdrEncoder {

// libultrahdr 多数 API 通过 uhdr_error_info_t 返回错误；统一转成 expected 便于上层处理。
auto check_uhdr_status(const uhdr_error_info_t& status, std::string_view operation)
    -> std::expected<void, std::string> {
  if (status.error_code == UHDR_CODEC_OK) {
    return {};
  }

  if (status.has_detail) {
    return std::unexpected(std::format("{} failed: {}", operation, status.detail));
  }

  return std::unexpected(std::format("{} failed with error code {}", operation,
                                     std::to_underlying(status.error_code)));
}

// Ultra HDR JPEG：同一张图需要「SDR 底图（8-bit sRGB）」+「HDR 线性半精度图层」，
// 库根据两者生成 JPEG 主图 + 增益图并在容器里封装。
auto encode_ultrahdr_jpeg(const std::vector<std::uint16_t>& source_pixels, std::uint32_t width,
                          std::uint32_t height, const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  auto encoder = std::unique_ptr<uhdr_codec_private_t, decltype(&uhdr_release_encoder)>(
      uhdr_create_encoder(), uhdr_release_encoder);
  if (!encoder) {
    return std::unexpected("Failed to create Ultra HDR encoder");
  }

  constexpr std::size_t kChannelsPerPixel = 4;
  std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(width) * height;
  if (pixel_count_u64 > std::numeric_limits<std::size_t>::max() / kChannelsPerPixel) {
    return std::unexpected("HDR image is too large");
  }

  std::size_t expected_bytes = static_cast<std::size_t>(pixel_count_u64) * kChannelsPerPixel;
  if (source_pixels.size() != expected_bytes) {
    return std::unexpected("HDR pixel buffer size mismatch");
  }

  // SDR：对 HDR 场景做 tone map 后再 sRGB 编码；HDR：线性半精度，见
  // pack_ultrahdr_hdr_rgb_half_plane。
  auto sdr_pixels_result = build_sdr_base_pixels(source_pixels);
  if (!sdr_pixels_result) {
    return std::unexpected(sdr_pixels_result.error());
  }

  auto hdr_pixels_result = pack_ultrahdr_hdr_rgb_half_plane(source_pixels);
  if (!hdr_pixels_result) {
    return std::unexpected(hdr_pixels_result.error());
  }

  auto& hdr_pixels = hdr_pixels_result.value();
  auto& sdr_pixels = sdr_pixels_result.value();

  // HDR 侧：scene-linear BT.709，全用 A 通道为 1.0（half 的 1.0）的典型写法。
  uhdr_raw_image_t hdr_image{};
  hdr_image.fmt = UHDR_IMG_FMT_64bppRGBAHalfFloat;
  hdr_image.cg = UHDR_CG_BT_709;
  hdr_image.ct = UHDR_CT_LINEAR;
  hdr_image.range = UHDR_CR_FULL_RANGE;
  hdr_image.w = width;
  hdr_image.h = height;
  hdr_image.planes[UHDR_PLANE_PACKED] = hdr_pixels.data();
  hdr_image.stride[UHDR_PLANE_PACKED] = width;

  if (auto status =
          check_uhdr_status(uhdr_enc_set_raw_image(encoder.get(), &hdr_image, UHDR_HDR_IMG),
                            "uhdr_enc_set_raw_image(hdr)");
      !status) {
    return std::unexpected(status.error());
  }

  // SDR 侧：已将线性结果压到 0–1 并做 sRGB OETF，故这里标为 sRGB transfer。
  uhdr_raw_image_t sdr_image{};
  sdr_image.fmt = UHDR_IMG_FMT_32bppRGBA8888;
  sdr_image.cg = UHDR_CG_BT_709;
  sdr_image.ct = UHDR_CT_SRGB;
  sdr_image.range = UHDR_CR_FULL_RANGE;
  sdr_image.w = width;
  sdr_image.h = height;
  sdr_image.planes[UHDR_PLANE_PACKED] = sdr_pixels.data();
  sdr_image.stride[UHDR_PLANE_PACKED] = width;

  if (auto status =
          check_uhdr_status(uhdr_enc_set_raw_image(encoder.get(), &sdr_image, UHDR_SDR_IMG),
                            "uhdr_enc_set_raw_image(sdr)");
      !status) {
    return std::unexpected(status.error());
  }

  int base_quality = std::clamp(options.base_quality, 0, 100);
  int gainmap_quality = std::clamp(options.gainmap_quality, 0, 100);
  // 底图与增益图可分别控质量：底图决定 SDR 观感细节，增益图影响 HDR 还原精细度。
  if (auto status =
          check_uhdr_status(uhdr_enc_set_quality(encoder.get(), base_quality, UHDR_BASE_IMG),
                            "uhdr_enc_set_quality(base)");
      !status) {
    return std::unexpected(status.error());
  }
  if (auto status =
          check_uhdr_status(uhdr_enc_set_quality(encoder.get(), gainmap_quality, UHDR_GAIN_MAP_IMG),
                            "uhdr_enc_set_quality(gainmap)");
      !status) {
    return std::unexpected(status.error());
  }

  if (auto status = check_uhdr_status(uhdr_enc_set_output_format(encoder.get(), UHDR_CODEC_JPG),
                                      "uhdr_enc_set_output_format");
      !status) {
    return std::unexpected(status.error());
  }

  // 与显示器能力相关的元数据；203–10000 nits 与库侧常见约束一致。
  float peak_nits = std::clamp(options.target_display_peak_nits, 203.0f, 10000.0f);
  if (auto status =
          check_uhdr_status(uhdr_enc_set_target_display_peak_brightness(encoder.get(), peak_nits),
                            "uhdr_enc_set_target_display_peak_brightness");
      !status) {
    return std::unexpected(status.error());
  }

  if (auto status = check_uhdr_status(uhdr_enc_set_preset(encoder.get(), UHDR_USAGE_BEST_QUALITY),
                                      "uhdr_enc_set_preset");
      !status) {
    return std::unexpected(status.error());
  }

  if (auto status = check_uhdr_status(uhdr_encode(encoder.get()), "uhdr_encode"); !status) {
    return std::unexpected(status.error());
  }

  // libultrahdr 把 ICC 写在 libjpeg 尾流里；按与 tools/remux_uhdr_app_order.py 相同规则重排 SOS 前
  // APP 并修正 MPF。
  uhdr_compressed_image_t* output = uhdr_get_encoded_stream(encoder.get());
  if (!output || !output->data || output->data_sz == 0) {
    return std::unexpected("Ultra HDR encoder returned empty output");
  }

  auto* output_bytes = static_cast<std::uint8_t*>(output->data);
  std::vector<std::uint8_t> raw(output_bytes, output_bytes + output->data_sz);
  auto remuxed = Utils::Image::remux_uhdr_jpeg_app_order(raw);
  if (!remuxed) {
    return std::unexpected(remuxed.error());
  }
  return std::move(*remuxed);
}

// 将已是「完整 JPEG 文件字节」的 buffer 写到磁盘。
auto write_file(const std::wstring& file_path, const std::vector<std::uint8_t>& data)
    -> std::expected<void, std::string> {
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
