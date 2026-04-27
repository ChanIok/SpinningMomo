module;

#include <dkm.hpp>

module Features.Gallery.Color.Extractor;

import std;
import Utils.Image;
import Features.Gallery.Color.Types;

namespace Features::Gallery::Color::Extractor {

auto parse_hex_nibble(char ch) -> std::optional<uint8_t> {
  if (ch >= '0' && ch <= '9') {
    return static_cast<uint8_t>(ch - '0');
  }
  if (ch >= 'a' && ch <= 'f') {
    return static_cast<uint8_t>(10 + (ch - 'a'));
  }
  if (ch >= 'A' && ch <= 'F') {
    return static_cast<uint8_t>(10 + (ch - 'A'));
  }
  return std::nullopt;
}

auto parse_hex_byte(char high, char low) -> std::optional<uint8_t> {
  auto high_value = parse_hex_nibble(high);
  auto low_value = parse_hex_nibble(low);
  if (!high_value || !low_value) {
    return std::nullopt;
  }
  return static_cast<uint8_t>((*high_value << 4) | *low_value);
}

auto parse_hex_color(std::string_view hex) -> std::expected<std::array<uint8_t, 3>, std::string> {
  if (!hex.empty() && hex.front() == '#') {
    hex.remove_prefix(1);
  }
  if (hex.size() != 6) {
    return std::unexpected("Expected color format #RRGGBB or RRGGBB");
  }

  auto r = parse_hex_byte(hex[0], hex[1]);
  auto g = parse_hex_byte(hex[2], hex[3]);
  auto b = parse_hex_byte(hex[4], hex[5]);
  if (!r || !g || !b) {
    return std::unexpected("Invalid hex color characters");
  }

  return std::array<uint8_t, 3>{*r, *g, *b};
}

auto srgb_to_linear(float value) -> float {
  float normalized = value / 255.0f;
  if (normalized <= 0.04045f) {
    return normalized / 12.92f;
  }
  return std::pow((normalized + 0.055f) / 1.055f, 2.4f);
}

auto lab_f(float value) -> float {
  constexpr float epsilon = 216.0f / 24389.0f;
  constexpr float kappa = 24389.0f / 27.0f;
  if (value > epsilon) {
    return std::cbrt(value);
  }
  return (kappa * value + 16.0f) / 116.0f;
}

auto rgb_to_lab_color(uint8_t r, uint8_t g, uint8_t b, float l_bin_size, float ab_bin_size)
    -> Types::LabColor {
  float lr = srgb_to_linear(static_cast<float>(r));
  float lg = srgb_to_linear(static_cast<float>(g));
  float lb = srgb_to_linear(static_cast<float>(b));

  float x = lr * 0.4124564f + lg * 0.3575761f + lb * 0.1804375f;
  float y = lr * 0.2126729f + lg * 0.7151522f + lb * 0.0721750f;
  float z = lr * 0.0193339f + lg * 0.1191920f + lb * 0.9503041f;

  constexpr float ref_x = 0.95047f;
  constexpr float ref_y = 1.00000f;
  constexpr float ref_z = 1.08883f;

  float fx = lab_f(x / ref_x);
  float fy = lab_f(y / ref_y);
  float fz = lab_f(z / ref_z);

  float l = 116.0f * fy - 16.0f;
  float a = 500.0f * (fx - fy);
  float lab_b = 200.0f * (fy - fz);

  int l_bin = std::max(0, static_cast<int>(std::floor(l / l_bin_size)));
  int a_bin = std::max(0, static_cast<int>(std::floor((a + 128.0f) / ab_bin_size)));
  int b_bin = std::max(0, static_cast<int>(std::floor((lab_b + 128.0f) / ab_bin_size)));

  return Types::LabColor{
      .l = l,
      .a = a,
      .b = lab_b,
      .l_bin = l_bin,
      .a_bin = a_bin,
      .b_bin = b_bin,
  };
}

auto run_kmeans(const std::vector<std::array<float, 3>>& points, size_t k)
    -> std::expected<std::pair<std::vector<std::array<float, 3>>, std::vector<size_t>>,
                     std::string> {
  try {
    auto [means, labels] = dkm::kmeans_lloyd(points, k);
    std::vector<size_t> normalized_labels;
    normalized_labels.reserve(labels.size());
    for (const auto& label : labels) {
      normalized_labels.push_back(static_cast<size_t>(label));
    }
    return std::make_pair(std::move(means), std::move(normalized_labels));
  } catch (const std::exception& e) {
    return std::unexpected("DKM kmeans failed: " + std::string(e.what()));
  }
}

auto delta_e_76(const Types::ExtractedColor& lhs, const Types::ExtractedColor& rhs) -> float {
  float dl = lhs.lab_l - rhs.lab_l;
  float da = lhs.lab_a - rhs.lab_a;
  float db = lhs.lab_b - rhs.lab_b;
  return std::sqrt(dl * dl + da * da + db * db);
}

auto extract_main_colors_from_bgra(const Utils::Image::BGRABitmapData& bitmap_data,
                                   const Types::MainColorExtractOptions& options)
    -> std::expected<std::vector<Types::ExtractedColor>, std::string> {
  uint64_t total_pixels = static_cast<uint64_t>(bitmap_data.width) * bitmap_data.height;
  if (total_pixels == 0) {
    return std::unexpected("Image has no pixels");
  }

  uint32_t max_samples = options.max_samples;
  uint64_t sample_step = std::max<uint64_t>(1, total_pixels / max_samples);
  std::vector<std::array<float, 3>> points;
  points.reserve(static_cast<size_t>(std::min<uint64_t>(total_pixels, max_samples)));

  for (uint64_t index = 0; index < total_pixels; index += sample_step) {
    uint32_t y = static_cast<uint32_t>(index / bitmap_data.width);
    uint32_t x = static_cast<uint32_t>(index % bitmap_data.width);
    uint64_t offset = static_cast<uint64_t>(y) * bitmap_data.stride + static_cast<uint64_t>(x) * 4;
    if (offset + 3 >= bitmap_data.pixels.size()) {
      continue;
    }

    uint8_t b = bitmap_data.pixels[offset + 0];
    uint8_t g = bitmap_data.pixels[offset + 1];
    uint8_t r = bitmap_data.pixels[offset + 2];
    uint8_t a = bitmap_data.pixels[offset + 3];
    if (a < 16) {
      continue;
    }

    points.push_back({static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)});
  }

  if (points.empty()) {
    return std::unexpected("No valid pixels found for color extraction");
  }

  size_t cluster_count =
      std::clamp(static_cast<size_t>(options.cluster_count), size_t{1}, points.size());
  auto kmeans_result = run_kmeans(points, cluster_count);
  if (!kmeans_result) {
    return std::unexpected("Color clustering failed: " + kmeans_result.error());
  }

  auto [means, labels] = std::move(kmeans_result.value());
  std::vector<size_t> cluster_counts(means.size(), 0);
  for (auto label : labels) {
    if (label < cluster_counts.size()) {
      cluster_counts[label] += 1;
    }
  }

  std::vector<Types::ExtractedColor> palette;
  palette.reserve(means.size());
  float total_weight = static_cast<float>(labels.size());

  for (size_t cluster = 0; cluster < means.size(); ++cluster) {
    if (cluster_counts[cluster] == 0) {
      continue;
    }
    uint8_t r = static_cast<uint8_t>(std::clamp(std::lround(means[cluster][0]), 0l, 255l));
    uint8_t g = static_cast<uint8_t>(std::clamp(std::lround(means[cluster][1]), 0l, 255l));
    uint8_t b = static_cast<uint8_t>(std::clamp(std::lround(means[cluster][2]), 0l, 255l));
    auto lab = rgb_to_lab_color(r, g, b, options.l_bin_size, options.ab_bin_size);

    palette.push_back(Types::ExtractedColor{
        .r = r,
        .g = g,
        .b = b,
        .lab_l = lab.l,
        .lab_a = lab.a,
        .lab_b = lab.b,
        .weight = static_cast<float>(cluster_counts[cluster]) / total_weight,
        .l_bin = lab.l_bin,
        .a_bin = lab.a_bin,
        .b_bin = lab.b_bin,
    });
  }

  std::ranges::sort(palette,
                    [](const Types::ExtractedColor& lhs, const Types::ExtractedColor& rhs) {
                      return lhs.weight > rhs.weight;
                    });

  std::vector<Types::ExtractedColor> merged_palette;
  for (const auto& color : palette) {
    bool merged = false;
    for (auto& existing : merged_palette) {
      if (delta_e_76(existing, color) < options.merge_delta_e) {
        float new_weight = existing.weight + color.weight;
        if (new_weight > 0.0f) {
          auto mixed_channel = [new_weight](float lhs, float lhs_weight, float rhs,
                                            float rhs_weight) {
            return (lhs * lhs_weight + rhs * rhs_weight) / new_weight;
          };

          uint8_t mixed_r = static_cast<uint8_t>(std::clamp(
              std::lround(mixed_channel(existing.r, existing.weight, color.r, color.weight)), 0l,
              255l));
          uint8_t mixed_g = static_cast<uint8_t>(std::clamp(
              std::lround(mixed_channel(existing.g, existing.weight, color.g, color.weight)), 0l,
              255l));
          uint8_t mixed_b = static_cast<uint8_t>(std::clamp(
              std::lround(mixed_channel(existing.b, existing.weight, color.b, color.weight)), 0l,
              255l));
          auto lab =
              rgb_to_lab_color(mixed_r, mixed_g, mixed_b, options.l_bin_size, options.ab_bin_size);

          existing = Types::ExtractedColor{
              .r = mixed_r,
              .g = mixed_g,
              .b = mixed_b,
              .lab_l = lab.l,
              .lab_a = lab.a,
              .lab_b = lab.b,
              .weight = new_weight,
              .l_bin = lab.l_bin,
              .a_bin = lab.a_bin,
              .b_bin = lab.b_bin,
          };
        }
        merged = true;
        break;
      }
    }

    if (!merged) {
      merged_palette.push_back(color);
    }
  }

  std::ranges::sort(merged_palette,
                    [](const Types::ExtractedColor& lhs, const Types::ExtractedColor& rhs) {
                      return lhs.weight > rhs.weight;
                    });

  float merged_weight_sum = 0.0f;
  for (const auto& color : merged_palette) {
    merged_weight_sum += color.weight;
  }
  if (merged_weight_sum > 0.0f) {
    for (auto& color : merged_palette) {
      color.weight /= merged_weight_sum;
    }
  }

  std::vector<Types::ExtractedColor> selected_palette;
  float cumulative_weight = 0.0f;
  for (const auto& color : merged_palette) {
    if (selected_palette.size() >= options.max_colors) {
      break;
    }

    bool require_for_minimum = selected_palette.size() < options.min_colors;
    if (!require_for_minimum && color.weight < options.min_weight) {
      continue;
    }

    selected_palette.push_back(color);
    cumulative_weight += color.weight;
    if (selected_palette.size() >= options.min_colors && cumulative_weight >= options.coverage) {
      break;
    }
  }

  if (selected_palette.empty() && !merged_palette.empty()) {
    selected_palette.push_back(merged_palette.front());
  }

  float selected_weight_sum = 0.0f;
  for (const auto& color : selected_palette) {
    selected_weight_sum += color.weight;
  }
  if (selected_weight_sum > 0.0f) {
    for (auto& color : selected_palette) {
      color.weight /= selected_weight_sum;
    }
  }

  return selected_palette;
}

auto extract_main_colors(Utils::Image::WICFactory& factory, const std::filesystem::path& path,
                         const Types::MainColorExtractOptions& options)
    -> std::expected<std::vector<Types::ExtractedColor>, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }
  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  auto bitmap_data_result =
      Utils::Image::load_scaled_bgra_bitmap_data(factory.get(), path, options.sample_short_edge);
  if (!bitmap_data_result) {
    return std::unexpected(bitmap_data_result.error());
  }

  return extract_main_colors_from_bgra(bitmap_data_result.value(), options);
}

}  // namespace Features::Gallery::Color::Extractor
