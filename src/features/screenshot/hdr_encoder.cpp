module Features.Screenshot.HdrEncoder;

import std;
import Utils.Logger;

namespace Features::Screenshot::HdrEncoder {

auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                   const UltraHdrEncodeOptions& options)
    -> std::expected<void, std::string> {
  // 这个函数故意保持很薄，只负责串起“预处理 -> 编码 -> 写盘”三段。
  // 这样后面查性能或排错时，边界很清楚。
  auto total_start = std::chrono::steady_clock::now();

  // 旧路径会先把原始 half 纹理完整读回 CPU，再逐像素构造 SDR/HDR 图层。
  // 现在这里直接调用 GPU 预处理，CPU 只拿最终的 SDR base 和 Gray8 gain map。
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

  Logger().debug("HDR screenshot total: preprocess={} ms, encode={} ms, write={} ms, total={} ms",
                 preprocess_ms, encode_ms, write_ms, total_ms);
  return {};
}

}  // namespace Features::Screenshot::HdrEncoder
