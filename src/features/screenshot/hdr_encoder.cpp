#include "features/screenshot/hdr_encoder.hpp"

#include "vendor/std.hpp"

#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::screenshot::hdr_encoder {

auto save_prepared_images_as_ultrahdr_jpeg(const UltraHdrPreparedImages& prepared,
                                           const std::wstring& file_path,
                                           const UltraHdrEncodeOptions& options)
    -> std::expected<void, std::string> {
  // 这个函数只负责“编码 -> 写盘”；GPU 预处理由调用方单独安排，
  // 这样 JXR 的 CPU 编码可以与前置 GPU 阶段重叠。
  const auto total_start = std::chrono::steady_clock::now();

  // 编码阶段内部会完成：
  // 1) WIC base JPEG
  // 2) WIC gain map JPEG
  // 3) 本地写 XMP / ISO 21496-1 / MPF
  auto encode_start = std::chrono::steady_clock::now();
  auto encoded_result = encode_ultrahdr_jpeg(prepared, options);
  if (!encoded_result) {
    return std::unexpected(encoded_result.error());
  }
  const auto encode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - encode_start)
                             .count();

  auto write_start = std::chrono::steady_clock::now();
  auto write_result = write_file(file_path, encoded_result.value());
  if (!write_result) {
    return std::unexpected(write_result.error());
  }
  const auto write_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - write_start)
                            .count();
  const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - total_start)
                            .count();

  Logger().info(
      "HDR screenshot encoded: {}x{}, path={}, encode={} ms, write={} ms, total={} ms, "
      "output={} bytes, gain_range=[{:.4f}, {:.4f}], target_peak={:.0f} nits",
      prepared.width, prepared.height, utils::string::ToUtf8(file_path), encode_ms, write_ms,
      total_ms, encoded_result->size(), prepared.min_gain_log2, prepared.max_gain_log2,
      options.target_display_peak_nits);
  return {};
}

auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                   const UltraHdrEncodeOptions& options)
    -> std::expected<void, std::string> {
  auto preprocess_result = preprocess_texture_for_ultrahdr(texture);
  if (!preprocess_result) {
    return std::unexpected(preprocess_result.error());
  }
  return save_prepared_images_as_ultrahdr_jpeg(preprocess_result.value(), file_path, options);
}

}  // namespace features::screenshot::hdr_encoder
