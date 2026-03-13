module;

#include <dkm.hpp>

module Features.Settings.BackgroundAnalyzer;

import std;
import Features.Settings.Types;
import Utils.Image;
import Utils.Path;

namespace Features::Settings::BackgroundAnalyzer {

struct RgbColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};

struct HslColor {
  double h = 0.0;
  double s = 0.0;
  double l = 0.0;
};

// 分析前将图像短边缩放至此像素数，兼顾速度与精度
constexpr std::uint32_t kAnalysisSampleShortEdge = 320;
// 亮度计算的最大采样点数，避免对大图逐像素遍历
constexpr std::uint32_t kMaxBrightnessSamples = 14'000;
// 每个局部区域颜色分析的最大采样点数
constexpr std::uint32_t kMaxRegionSamples = 3'200;
// 全图主色估算的最大采样点数
constexpr std::uint32_t kMaxPrimarySamples = 8'000;
// 每个采样区域的边长占图像对应边长的比例（约 1/4）
constexpr float kRegionSizeRatio = 0.26f;
// 相对亮度达到或超过此阈值时判定为浅色主题
constexpr double kLightThemeThreshold = 0.48;
// K-Means 聚类数，用于提取区域主色
constexpr std::size_t kRegionClusterCount = 5;

auto rgb_to_hex(const RgbColor& color) -> std::string {
  return std::format("#{:02X}{:02X}{:02X}", color.r, color.g, color.b);
}

// IEC 61966-2-1 标准：将 sRGB 分量（0-255）转换为线性光强度
// 低值段用线性近似，高值段用伽马曲线还原
auto srgb_channel_to_linear(double channel) -> double {
  double normalized = channel / 255.0;
  if (normalized <= 0.04045) {
    return normalized / 12.92;
  }
  return std::pow((normalized + 0.055) / 1.055, 2.4);
}

// WCAG 2.x 相对亮度公式，系数来自 BT.709 标准（人眼对绿色最敏感）
auto relative_luminance(const RgbColor& color) -> double {
  double r = srgb_channel_to_linear(static_cast<double>(color.r));
  double g = srgb_channel_to_linear(static_cast<double>(color.g));
  double b = srgb_channel_to_linear(static_cast<double>(color.b));
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

auto rgb_to_hsl(const RgbColor& color) -> HslColor {
  double r = static_cast<double>(color.r) / 255.0;
  double g = static_cast<double>(color.g) / 255.0;
  double b = static_cast<double>(color.b) / 255.0;

  double max_value = std::max({r, g, b});
  double min_value = std::min({r, g, b});
  double delta = max_value - min_value;

  double h = 0.0;
  double l = (max_value + min_value) * 0.5;
  double s = 0.0;

  if (delta > 0.0) {
    s = delta / (1.0 - std::abs(2.0 * l - 1.0));

    if (max_value == r) {
      h = std::fmod((g - b) / delta, 6.0);
    } else if (max_value == g) {
      h = ((b - r) / delta) + 2.0;
    } else {
      h = ((r - g) / delta) + 4.0;
    }

    h *= 60.0;
    if (h < 0.0) {
      h += 360.0;
    }
  }

  return HslColor{
      .h = h,
      .s = s * 100.0,
      .l = l * 100.0,
  };
}

auto hsl_to_rgb(const HslColor& hsl) -> RgbColor {
  double saturation = std::clamp(hsl.s, 0.0, 100.0) / 100.0;
  double lightness = std::clamp(hsl.l, 0.0, 100.0) / 100.0;
  double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
  double hue_prime = std::fmod(hsl.h, 360.0) / 60.0;
  if (hue_prime < 0.0) hue_prime += 6.0;
  double x = chroma * (1.0 - std::abs(std::fmod(hue_prime, 2.0) - 1.0));

  double r = 0.0;
  double g = 0.0;
  double b = 0.0;

  if (hue_prime < 1.0) {
    r = chroma;
    g = x;
  } else if (hue_prime < 2.0) {
    r = x;
    g = chroma;
  } else if (hue_prime < 3.0) {
    g = chroma;
    b = x;
  } else if (hue_prime < 4.0) {
    g = x;
    b = chroma;
  } else if (hue_prime < 5.0) {
    r = x;
    b = chroma;
  } else {
    r = chroma;
    b = x;
  }

  double m = lightness - chroma * 0.5;
  auto to_channel = [](double value) -> std::uint8_t {
    long rounded = std::lround(value * 255.0);
    return static_cast<std::uint8_t>(std::clamp(rounded, 0l, 255l));
  };

  return RgbColor{
      .r = to_channel(r + m),
      .g = to_channel(g + m),
      .b = to_channel(b + m),
  };
}

// 根据目标亮度计算饱和度上限（倒 U 形曲线）。
// 在 L≈55% 时峰值约 85%，两端降至约 35%，使极亮和极暗颜色自然淡雅。
auto saturation_cap_for_lightness(double l) -> double {
  double t = (l - 55.0) / 55.0;
  return 35.0 + 50.0 * (1.0 - t * t);
}

// 将输入颜色的亮度钳制到目标区间，饱和度由目标亮度决定上限（只降不升）。
// 低饱和输入（S<12）视为灰色系，保持低饱和不推高色调。
auto compensate_for_theme(const RgbColor& color, std::string_view theme_mode, bool primary)
    -> RgbColor {
  auto hsl = rgb_to_hsl(color);

  double l_min, l_max;
  if (theme_mode == "light") {
    l_min = primary ? 35.0 : 80.0;
    l_max = primary ? 55.0 : 92.0;
  } else {
    l_min = primary ? 58.0 : 10.0;
    l_max = primary ? 72.0 : 22.0;
  }

  hsl.l = std::clamp(hsl.l, l_min, l_max);

  if (hsl.s < 12.0) {
    hsl.s = std::min(hsl.s, 20.0);
  } else {
    double s_max = saturation_cap_for_lightness(hsl.l);
    if (!primary) s_max *= 0.6;
    hsl.s = std::min(hsl.s, s_max);
  }

  return hsl_to_rgb(hsl);
}

auto normalize_wallpaper_input_path(std::string_view raw_path) -> std::string {
  std::string normalized(raw_path);
  for (auto& ch : normalized) {
    if (ch == '\\') ch = '/';
  }

  auto query_pos = normalized.find_first_of("?#");
  if (query_pos != std::string::npos) {
    normalized = normalized.substr(0, query_pos);
  }

  auto trim = [](std::string& value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && is_space(value.front())) {
      value.erase(value.begin());
    }
    while (!value.empty() && is_space(value.back())) {
      value.pop_back();
    }
  };
  trim(normalized);
  return normalized;
}

auto resolve_wallpaper_path(std::string_view raw_path)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized = normalize_wallpaper_input_path(raw_path);
  if (normalized.empty()) {
    return std::unexpected("Wallpaper path is empty");
  }

  std::filesystem::path direct_path(normalized);
  if (direct_path.is_absolute() && std::filesystem::exists(direct_path)) {
    return std::filesystem::weakly_canonical(direct_path);
  }

  std::string local_path = normalized;
  if (local_path.starts_with("/assets/")) {
    local_path = "./resources/web" + local_path;
  } else if (local_path.starts_with("assets/")) {
    local_path = "./resources/web/" + local_path;
  } else if (local_path.starts_with("/")) {
    local_path = "." + local_path;
  }

  auto resolved_result = Utils::Path::NormalizePath(std::filesystem::path(local_path));
  if (!resolved_result) {
    return std::unexpected("Failed to normalize wallpaper path: " + resolved_result.error());
  }

  auto resolved_path = resolved_result.value();
  if (!std::filesystem::exists(resolved_path)) {
    return std::unexpected("Wallpaper file does not exist: " + resolved_path.string());
  }

  return resolved_path;
}

auto load_wallpaper_bitmap(const std::filesystem::path& path)
    -> std::expected<Utils::Image::BGRABitmapData, std::string> {
  auto wic_result = Utils::Image::get_thread_wic_factory();
  if (!wic_result) {
    return std::unexpected("Failed to initialize WIC factory: " + wic_result.error());
  }

  auto frame_result = Utils::Image::load_bitmap_frame(wic_result->get(), path);
  if (!frame_result) {
    return std::unexpected(frame_result.error());
  }

  auto scaled_result =
      Utils::Image::scale_bitmap(wic_result->get(), frame_result->get(), kAnalysisSampleShortEdge);
  if (!scaled_result) {
    return std::unexpected(scaled_result.error());
  }

  auto bgra_result = Utils::Image::convert_to_bgra_bitmap(wic_result->get(), scaled_result->get());
  if (!bgra_result) {
    return std::unexpected(bgra_result.error());
  }

  auto bitmap_data_result = Utils::Image::copy_bgra_bitmap_data(bgra_result->get());
  if (!bitmap_data_result) {
    return std::unexpected(bitmap_data_result.error());
  }

  return bitmap_data_result.value();
}

// 从矩形区域内均匀采样像素点（RGB 三元组）。
// pixel_step 由区域面积与最大采样数的比值开方得出，保证空间覆盖均匀且总量不超限。
// alpha < 16 的近透明像素被跳过，避免影响颜色统计。
auto collect_points_in_rect(const Utils::Image::BGRABitmapData& bitmap, int x0, int y0, int x1,
                            int y1, std::uint32_t max_samples)
    -> std::vector<std::array<float, 3>> {
  std::vector<std::array<float, 3>> points;
  if (x0 >= x1 || y0 >= y1) {
    return points;
  }

  std::uint64_t area = static_cast<std::uint64_t>(x1 - x0) * static_cast<std::uint64_t>(y1 - y0);
  int pixel_step =
      std::max(1, static_cast<int>(std::sqrt(static_cast<double>(area) / max_samples)));

  points.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(area, max_samples)));
  for (int y = y0; y < y1; y += pixel_step) {
    for (int x = x0; x < x1; x += pixel_step) {
      std::uint64_t offset =
          static_cast<std::uint64_t>(y) * bitmap.stride + static_cast<std::uint64_t>(x) * 4;
      std::uint8_t b = bitmap.pixels[offset + 0];
      std::uint8_t g = bitmap.pixels[offset + 1];
      std::uint8_t r = bitmap.pixels[offset + 2];
      std::uint8_t a = bitmap.pixels[offset + 3];
      if (a < 16) continue;

      points.push_back({static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)});
    }
  }

  return points;
}

auto average_color_from_points(const std::vector<std::array<float, 3>>& points) -> RgbColor {
  if (points.empty()) {
    return RgbColor{};
  }

  double sum_r = 0.0;
  double sum_g = 0.0;
  double sum_b = 0.0;
  for (const auto& point : points) {
    sum_r += point[0];
    sum_g += point[1];
    sum_b += point[2];
  }

  auto to_channel = [count = static_cast<double>(points.size())](double value) -> std::uint8_t {
    long rounded = std::lround(value / count);
    return static_cast<std::uint8_t>(std::clamp(rounded, 0l, 255l));
  };

  return RgbColor{
      .r = to_channel(sum_r),
      .g = to_channel(sum_g),
      .b = to_channel(sum_b),
  };
}

// 使用 K-Means（Lloyd 算法）对像素点聚类，返回像素数最多的簇的中心色作为主色。
// 当点集过少无法聚类，或 K-Means 抛出异常时，回退到简单算术平均色。
auto dominant_color_from_points(const std::vector<std::array<float, 3>>& points)
    -> std::expected<RgbColor, std::string> {
  if (points.empty()) {
    return std::unexpected("No valid pixels found in analysis region");
  }

  std::size_t cluster_count = std::clamp(kRegionClusterCount, std::size_t{1}, points.size());
  try {
    auto [means, labels] = dkm::kmeans_lloyd(points, cluster_count);
    std::vector<std::size_t> counts(means.size(), 0);
    for (const auto& label : labels) {
      counts[static_cast<std::size_t>(label)] += 1;
    }

    auto max_it = std::ranges::max_element(counts);
    std::size_t dominant_index = static_cast<std::size_t>(std::distance(counts.begin(), max_it));
    auto mean = means[dominant_index];

    return RgbColor{
        .r = static_cast<std::uint8_t>(std::clamp(std::lround(mean[0]), 0l, 255l)),
        .g = static_cast<std::uint8_t>(std::clamp(std::lround(mean[1]), 0l, 255l)),
        .b = static_cast<std::uint8_t>(std::clamp(std::lround(mean[2]), 0l, 255l)),
    };
  } catch (...) {
    return average_color_from_points(points);
  }
}

auto sample_region_color(const Utils::Image::BGRABitmapData& bitmap, float x_ratio, float y_ratio)
    -> std::expected<RgbColor, std::string> {
  int width = static_cast<int>(bitmap.width);
  int height = static_cast<int>(bitmap.height);
  if (width <= 0 || height <= 0) {
    return std::unexpected("Wallpaper bitmap is empty");
  }

  int region_width = std::max(8, static_cast<int>(std::lround(bitmap.width * kRegionSizeRatio)));
  int region_height = std::max(8, static_cast<int>(std::lround(bitmap.height * kRegionSizeRatio)));
  int center_x = static_cast<int>(std::lround(bitmap.width * x_ratio));
  int center_y = static_cast<int>(std::lround(bitmap.height * y_ratio));

  int x0 = std::clamp(center_x - region_width / 2, 0, width - 1);
  int y0 = std::clamp(center_y - region_height / 2, 0, height - 1);
  int x1 = std::clamp(x0 + region_width, 1, width);
  int y1 = std::clamp(y0 + region_height, 1, height);

  auto points = collect_points_in_rect(bitmap, x0, y0, x1, y1, kMaxRegionSamples);
  return dominant_color_from_points(points);
}

auto estimate_primary_color(const Utils::Image::BGRABitmapData& bitmap)
    -> std::expected<RgbColor, std::string> {
  auto points = collect_points_in_rect(bitmap, 0, 0, static_cast<int>(bitmap.width),
                                       static_cast<int>(bitmap.height), kMaxPrimarySamples);
  return dominant_color_from_points(points);
}

// 计算壁纸的平均相对亮度，以 alpha 通道作为加权系数，忽略完全透明像素。
// 使用固定步长均匀采样，将计算量控制在 kMaxBrightnessSamples 以内。
auto compute_wallpaper_brightness(const Utils::Image::BGRABitmapData& bitmap) -> double {
  std::uint64_t total_pixels = static_cast<std::uint64_t>(bitmap.width) * bitmap.height;
  if (total_pixels == 0) {
    return 0.0;
  }

  std::uint64_t step = std::max<std::uint64_t>(1, total_pixels / kMaxBrightnessSamples);
  double total = 0.0;
  double alpha_weight = 0.0;

  for (std::uint64_t index = 0; index < total_pixels; index += step) {
    std::uint32_t y = static_cast<std::uint32_t>(index / bitmap.width);
    std::uint32_t x = static_cast<std::uint32_t>(index % bitmap.width);
    std::uint64_t offset =
        static_cast<std::uint64_t>(y) * bitmap.stride + static_cast<std::uint64_t>(x) * 4;
    double alpha = static_cast<double>(bitmap.pixels[offset + 3]) / 255.0;
    if (alpha <= 0.0) continue;

    RgbColor color{
        .r = bitmap.pixels[offset + 2],
        .g = bitmap.pixels[offset + 1],
        .b = bitmap.pixels[offset + 0],
    };
    total += relative_luminance(color) * alpha;
    alpha_weight += alpha;
  }

  if (alpha_weight <= 0.0) {
    return 0.0;
  }

  return std::clamp(total / alpha_weight, 0.0, 1.0);
}

auto resolve_theme_mode(double brightness) -> std::string {
  return brightness >= kLightThemeThreshold ? "light" : "dark";
}

// 返回叠加层颜色的采样锚点（图像相对坐标），坐标以图像宽高百分比表示。
// mode 1：仅取中心；mode 2：对角两点；mode 3：对角线三点；mode 4（默认）：四角均匀分布。
auto overlay_sample_points(int mode) -> std::vector<std::pair<float, float>> {
  switch (mode) {
    case 1:
      return {{0.5f, 0.5f}};
    case 2:
      return {
          {0.2f, 0.2f},
          {0.8f, 0.8f},
      };
    case 3:
      return {
          {0.18f, 0.2f},
          {0.5f, 0.5f},
          {0.82f, 0.8f},
      };
    case 4:
    default:
      return {
          {0.18f, 0.2f},
          {0.82f, 0.2f},
          {0.82f, 0.8f},
          {0.18f, 0.8f},
      };
  }
}

// 壁纸分析主入口：解析路径 → 加载并缩放位图 → 计算亮度/主题 → 提取各锚点叠加色和全图主色，
// 所有颜色均经过主题补偿后以十六进制字符串返回。
auto analyze_background(const Types::AnalyzeBackgroundParams& params)
    -> std::expected<Types::AnalyzeBackgroundResult, std::string> {
  auto path_result = resolve_wallpaper_path(params.image_path);
  if (!path_result) {
    return std::unexpected(path_result.error());
  }

  auto bitmap_result = load_wallpaper_bitmap(path_result.value());
  if (!bitmap_result) {
    return std::unexpected("Failed to load wallpaper bitmap: " + bitmap_result.error());
  }
  auto bitmap = std::move(bitmap_result.value());

  double brightness = compute_wallpaper_brightness(bitmap);
  auto theme_mode = resolve_theme_mode(brightness);

  int overlay_mode = std::clamp(params.overlay_mode, 1, 4);
  auto sample_points = overlay_sample_points(overlay_mode);

  std::vector<std::string> overlay_colors;
  overlay_colors.reserve(sample_points.size());
  for (const auto& [x_ratio, y_ratio] : sample_points) {
    auto region_color = sample_region_color(bitmap, x_ratio, y_ratio);
    if (!region_color) {
      return std::unexpected(region_color.error());
    }
    auto compensated = compensate_for_theme(region_color.value(), theme_mode, false);
    overlay_colors.push_back(rgb_to_hex(compensated));
  }

  auto primary_color = estimate_primary_color(bitmap);
  if (!primary_color) {
    return std::unexpected(primary_color.error());
  }

  auto compensated_primary = compensate_for_theme(primary_color.value(), theme_mode, true);

  return Types::AnalyzeBackgroundResult{
      .theme_mode = theme_mode,
      .primary_color = rgb_to_hex(compensated_primary),
      .overlay_colors = std::move(overlay_colors),
      .brightness = brightness,
  };
}

}  // namespace Features::Settings::BackgroundAnalyzer
