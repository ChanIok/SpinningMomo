#pragma once

#include "vendor/std.hpp"

#include "features/gallery/color/types.hpp"
#include "utils/image/image.hpp"

namespace features::gallery::color::extractor {

auto parse_hex_color(std::string_view hex)
    -> std::expected<std::array<std::uint8_t, 3>, std::string>;

auto rgb_to_lab_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, float l_bin_size = 5.0f,
                      float ab_bin_size = 8.0f) -> LabColor;

auto extract_main_colors_from_bgra(const utils::image::BGRABitmapData& bitmap_data,
                                   const MainColorExtractOptions& options = {})
    -> std::expected<std::vector<ExtractedColor>, std::string>;

auto extract_main_colors(utils::image::WICFactory& factory, const std::filesystem::path& path,
                         const MainColorExtractOptions& options = {})
    -> std::expected<std::vector<ExtractedColor>, std::string>;

}  // namespace features::gallery::color::extractor
