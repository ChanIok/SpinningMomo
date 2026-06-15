module;

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

// 将 Lab 坐标量化到 bin，供按色筛选 SQL 做粗匹配
auto bin_lab_color(const Utils::Image::LabColor& lab, float l_bin_size, float ab_bin_size)
    -> Types::LabColor {
  int l_bin = std::max(0, static_cast<int>(std::floor(lab.l / l_bin_size)));
  int a_bin = std::max(0, static_cast<int>(std::floor((lab.a + 128.0f) / ab_bin_size)));
  int b_bin = std::max(0, static_cast<int>(std::floor((lab.b + 128.0f) / ab_bin_size)));

  return Types::LabColor{
      .l = lab.l,
      .a = lab.a,
      .b = lab.b,
      .l_bin = l_bin,
      .a_bin = a_bin,
      .b_bin = b_bin,
  };
}

auto rgb_to_lab_color(uint8_t r, uint8_t g, uint8_t b, float l_bin_size, float ab_bin_size)
    -> Types::LabColor {
  return bin_lab_color(Utils::Image::rgb_to_lab_color(r, g, b), l_bin_size, ab_bin_size);
}

auto delta_e_76(const Types::ExtractedColor& lhs, const Types::ExtractedColor& rhs) -> float {
  float dl = lhs.lab_l - rhs.lab_l;
  float da = lhs.lab_a - rhs.lab_a;
  float db = lhs.lab_b - rhs.lab_b;
  return std::sqrt(dl * dl + da * da + db * db);
}

// 从 BGRA 位图提取入库主色：聚类 → 合并相近色 → 按覆盖率筛选
auto extract_main_colors_from_bgra(const Utils::Image::BGRABitmapData& bitmap_data,
                                   const Types::MainColorExtractOptions& options)
    -> std::expected<std::vector<Types::ExtractedColor>, std::string> {
  auto palette_result = Utils::Image::extract_lab_palette_from_bgra_rect(
      bitmap_data, 0, 0, static_cast<int>(bitmap_data.width), static_cast<int>(bitmap_data.height),
      Utils::Image::PaletteExtractOptions{
          .max_samples = options.max_samples,
          .cluster_count = options.cluster_count,
      });
  if (!palette_result) {
    return std::unexpected("Color clustering failed: " + palette_result.error());
  }

  std::vector<Types::ExtractedColor> palette;
  palette.reserve(palette_result->size());

  // 将通用调色板转为 Gallery 入库结构，并量化 Lab bin 供按色筛选
  for (const auto& color : palette_result.value()) {
    auto lab = bin_lab_color(color.lab, options.l_bin_size, options.ab_bin_size);

    palette.push_back(Types::ExtractedColor{
        .r = color.rgb.r,
        .g = color.rgb.g,
        .b = color.rgb.b,
        .lab_l = lab.l,
        .lab_a = lab.a,
        .lab_b = lab.b,
        .weight = color.weight,
        .l_bin = lab.l_bin,
        .a_bin = lab.a_bin,
        .b_bin = lab.b_bin,
    });
  }

  std::vector<Types::ExtractedColor> merged_palette;
  for (const auto& color : palette) {
    bool merged = false;
    for (auto& existing : merged_palette) {
      // 感知距离足够近则合并，减少入库的近似重复色
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
          // Lab 从混合后的 RGB 重算，保证展示色与按色筛选坐标一致
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
  // 合并后归一化权重，coverage 筛选基于 0-1 占比
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
    // 已满足最少色数后，跳过占比过低的次要色
    if (!require_for_minimum && color.weight < options.min_weight) {
      continue;
    }

    selected_palette.push_back(color);
    cumulative_weight += color.weight;
    // 达到最少色数且累计覆盖率达标即可提前结束
    if (selected_palette.size() >= options.min_colors && cumulative_weight >= options.coverage) {
      break;
    }
  }

  // 兜底：至少保留权重最高的一色，避免空结果
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

// 缩放解码后提取主色；无现成 bitmap 时按 sample_short_edge 加载
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
