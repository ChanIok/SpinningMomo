module Features.Screenshot.HdrEncoder;

import std;

namespace Features::Screenshot::HdrEncoder {

auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                   const UltraHdrEncodeOptions& options)
    -> std::expected<void, std::string> {
  // 1) GPU → CPU：紧排 RGBA16F 缓冲区；2) CPU 编码；3) 落盘。
  auto mapped_result = map_texture_to_tight_rgba_half(texture);
  if (!mapped_result) {
    return std::unexpected(mapped_result.error());
  }

  auto [pixels, width, height] = std::move(mapped_result.value());
  auto encoded_result = encode_ultrahdr_jpeg(pixels, width, height, options);
  if (!encoded_result) {
    return std::unexpected(encoded_result.error());
  }

  return write_file(file_path, encoded_result.value());
}

}  // namespace Features::Screenshot::HdrEncoder
