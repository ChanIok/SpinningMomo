#pragma once

#include "features/gallery/color/types.hpp"
#include "utils/image/image.hpp"

namespace Features::Gallery::Color::Extractor {

auto parse_hex_color(std::string_view hex)
    -> std::expected<std::array<std::uint8_t, 3>, std::string>;

auto rgb_to_lab_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, float l_bin_size = 5.0f,
                      float ab_bin_size = 8.0f) -> Types::LabColor;

auto extract_main_colors_from_bgra(const Utils::Image::BGRABitmapData& bitmap_data,
                                   const Types::MainColorExtractOptions& options = {})
    -> std::expected<std::vector<Types::ExtractedColor>, std::string>;

auto extract_main_colors(Utils::Image::WICFactory& factory, const std::filesystem::path& path,
                         const Types::MainColorExtractOptions& options = {})
    -> std::expected<std::vector<Types::ExtractedColor>, std::string>;

}  // namespace Features::Gallery::Color::Extractor
