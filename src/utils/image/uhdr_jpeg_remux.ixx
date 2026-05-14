module;

export module Utils.Image.UhdrJpegRemux;

import std;

namespace Utils::Image {

// 将 libultrahdr 产出的 Ultra HDR JPEG 的 SOS 前 APP 重排为常见顺序（JFIF→Exif→XMP→ICC→ISO→MPF），
// 并修正 MPF 中 primary_image_size / secondary_image_offset；与 tools/remux_uhdr_app_order.py
// 语义一致。
export auto remux_uhdr_jpeg_app_order(std::span<const std::uint8_t> jpeg)
    -> std::expected<std::vector<std::uint8_t>, std::string>;

}  // namespace Utils::Image
