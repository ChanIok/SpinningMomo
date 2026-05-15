module Features.Screenshot.HdrEncoder;

import std;
import Utils.Logger;
import Utils.String;

namespace Features::Screenshot::HdrEncoder {

auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                   const UltraHdrEncodeOptions& options)
    -> std::expected<void, std::string> {
  // 这个函数故意保持很薄，只负责串起“预处理 -> 编码 -> 写盘”三段。
  // 这样后面查性能或排错时，边界很清楚。
  auto total_start = std::chrono::steady_clock::now();

  // 旧路径会先把原始 half 纹理完整读回 CPU，再逐像素构造 SDR/HDR 图层。
  // 现在这里直接调用 GPU 预处理；gain 链路跑完后再读回 SDR base 与量化后的 gain map。
  auto preprocess_start = std::chrono::steady_clock::now();
  auto prepared_result = preprocess_texture_for_ultrahdr(texture);
  if (!prepared_result) {
    return std::unexpected(prepared_result.error());
  }
  auto preprocess_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - preprocess_start)
                           .count();

  // 编码阶段内部会完成：
  // 1) WIC base JPEG
  // 2) WIC gain map JPEG
  // 3) 本地写 XMP / ISO 21496-1 / MPF
  auto encode_start = std::chrono::steady_clock::now();
  auto encoded_result = encode_ultrahdr_jpeg(prepared_result.value(), options);
  if (!encoded_result) {
    return std::unexpected(encoded_result.error());
  }
  auto encode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - encode_start)
                       .count();

  auto write_start = std::chrono::steady_clock::now();
  auto write_result = write_file(file_path, encoded_result.value());
  if (!write_result) {
    return std::unexpected(write_result.error());
  }
  auto write_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - write_start)
                      .count();
  auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - total_start)
                      .count();

  const auto& prepared = prepared_result.value();
  Logger().info(
      "HDR screenshot encoded: {}x{}, path={}, preprocess={} ms, encode={} ms, write={} ms, "
      "total={} ms, output={} bytes, gain_range=[{:.4f}, {:.4f}], target_peak={:.0f} nits",
      prepared.width, prepared.height, Utils::String::ToUtf8(file_path), preprocess_ms, encode_ms,
      write_ms, total_ms, encoded_result->size(), prepared.min_gain_log2, prepared.max_gain_log2,
      options.target_display_peak_nits);
  return {};
}

}  // namespace Features::Screenshot::HdrEncoder
