module;

#include <ultrahdr_api.h>
#include <windows.h>

module Features.Screenshot.HdrEncoder;

import std;
import Utils.Logger;
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
auto encode_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                          const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  auto encoder = std::unique_ptr<uhdr_codec_private_t, decltype(&uhdr_release_encoder)>(
      uhdr_create_encoder(), uhdr_release_encoder);
  if (!encoder) {
    return std::unexpected("Failed to create Ultra HDR encoder");
  }

  constexpr std::size_t kChannelsPerPixel = 4;
  std::uint64_t pixel_count_u64 = static_cast<std::uint64_t>(images.width) * images.height;
  if (pixel_count_u64 > std::numeric_limits<std::size_t>::max() / kChannelsPerPixel) {
    return std::unexpected("HDR image is too large");
  }

  std::size_t expected_sdr_bytes = static_cast<std::size_t>(pixel_count_u64) * kChannelsPerPixel;
  std::size_t expected_hdr_values = static_cast<std::size_t>(pixel_count_u64) * kChannelsPerPixel;
  if (images.sdr_pixels.size() != expected_sdr_bytes ||
      images.hdr_pixels.size() != expected_hdr_values) {
    return std::unexpected("Ultra HDR prepared image buffer size mismatch");
  }

  // 这里不再生成 SDR/HDR 像素，只校验 GPU 预处理结果并把指针传给 libultrahdr。
  // 两个 vector 在 uhdr_encode() 返回前必须保持存活，因此都由 images 持有到函数结束。

  // HDR 侧：GPU 已生成 scene-linear BT.709，A 通道为 1.0（half 的 1.0）。
  uhdr_raw_image_t hdr_image{};
  hdr_image.fmt = UHDR_IMG_FMT_64bppRGBAHalfFloat;
  hdr_image.cg = UHDR_CG_BT_709;
  hdr_image.ct = UHDR_CT_LINEAR;
  hdr_image.range = UHDR_CR_FULL_RANGE;
  hdr_image.w = images.width;
  hdr_image.h = images.height;
  hdr_image.planes[UHDR_PLANE_PACKED] = const_cast<std::uint16_t*>(images.hdr_pixels.data());
  hdr_image.stride[UHDR_PLANE_PACKED] = images.width;

  if (auto status =
          check_uhdr_status(uhdr_enc_set_raw_image(encoder.get(), &hdr_image, UHDR_HDR_IMG),
                            "uhdr_enc_set_raw_image(hdr)");
      !status) {
    return std::unexpected(status.error());
  }

  // SDR 侧：GPU 已将线性结果压到 0–1 并做 sRGB OETF，故这里标为 sRGB transfer。
  uhdr_raw_image_t sdr_image{};
  sdr_image.fmt = UHDR_IMG_FMT_32bppRGBA8888;
  sdr_image.cg = UHDR_CG_BT_709;
  sdr_image.ct = UHDR_CT_SRGB;
  sdr_image.range = UHDR_CR_FULL_RANGE;
  sdr_image.w = images.width;
  sdr_image.h = images.height;
  sdr_image.planes[UHDR_PLANE_PACKED] = const_cast<std::uint8_t*>(images.sdr_pixels.data());
  sdr_image.stride[UHDR_PLANE_PACKED] = images.width;

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

  // 质量目标优先，继续使用 best quality；性能优化集中在 libultrahdr 前的 GPU 预处理。
  auto encode_start = std::chrono::steady_clock::now();
  if (auto status = check_uhdr_status(uhdr_encode(encoder.get()), "uhdr_encode"); !status) {
    return std::unexpected(status.error());
  }
  auto encode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - encode_start)
                       .count();

  // libultrahdr 把 ICC 写在 libjpeg 尾流里；按与 tools/remux_uhdr_app_order.py 相同规则重排 SOS 前
  // APP 并修正 MPF。
  uhdr_compressed_image_t* output = uhdr_get_encoded_stream(encoder.get());
  if (!output || !output->data || output->data_sz == 0) {
    return std::unexpected("Ultra HDR encoder returned empty output");
  }

  auto* output_bytes = static_cast<std::uint8_t*>(output->data);
  std::vector<std::uint8_t> raw(output_bytes, output_bytes + output->data_sz);

  auto remux_start = std::chrono::steady_clock::now();
  auto remuxed = Utils::Image::remux_uhdr_jpeg_app_order(raw);
  if (!remuxed) {
    return std::unexpected(remuxed.error());
  }
  auto remux_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - remux_start)
                      .count();
  Logger().debug("HDR UHDR encode: libultrahdr={} ms, remux={} ms, output={} bytes", encode_ms,
                 remux_ms, remuxed->size());
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
