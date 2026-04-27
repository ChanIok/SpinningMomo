module;

#include <dkm.hpp>

module Features.Settings.Background;

import std;
import Core.State;
import Core.HttpServer.Static;
import Core.HttpServer.Types;
import Core.WebView.State;
import Core.WebView.Static;
import Core.WebView.Types;
import Features.Settings.Types;
import Utils.Image;
import Utils.Logger;
import Utils.Path;

namespace Features::Settings::Background {

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
constexpr std::string_view kBackgroundWebPrefix = "/static/backgrounds/";
constexpr std::wstring_view kBackgroundWebPrefixW = L"/static/backgrounds/";
constexpr std::string_view kBackgroundFileName = "background";

// 将外部传入的路径统一成模块内部使用的格式：
// 1. 统一分隔符为 '/'
// 2. 去掉 query / fragment
// 3. 去掉首尾空白
// 这样后续逻辑只需处理一种规范形态。
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

// 标准化路径用于比较，统一为小写和通用分隔符
auto normalize_compare_path(const std::filesystem::path& path) -> std::wstring {
  auto value = path.generic_wstring();
  std::transform(value.begin(), value.end(), value.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return value;
}

// 检查目标路径是否在基目录范围内，防止路径逃逸
auto is_path_within_base(const std::filesystem::path& target, const std::filesystem::path& base)
    -> bool {
  auto normalized_base = normalize_compare_path(base.lexically_normal());
  auto normalized_target = normalize_compare_path(target.lexically_normal());
  return normalized_target == normalized_base ||
         (normalized_target.size() > normalized_base.size() &&
          normalized_target.starts_with(normalized_base) &&
          normalized_target[normalized_base.size()] == L'/');
}

// 检查文件扩展名是否为支持的背景图片格式
auto is_supported_background_extension(std::string extension) -> bool {
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
         extension == ".bmp" || extension == ".gif" || extension == ".webp";
}

// 获取标准化的背景图片扩展名，不支持的默认为 .jpg
auto normalize_background_extension(const std::filesystem::path& path) -> std::string {
  auto extension = path.extension().generic_string();
  return is_supported_background_extension(extension) ? extension : ".jpg";
}

// 从 URL 中提取相对路径（泛型版本）
template <typename CharT>
auto extract_relative_path_generic(std::basic_string_view<CharT> url,
                                   std::basic_string_view<CharT> prefix)
    -> std::optional<std::basic_string<CharT>> {
  if (!url.starts_with(prefix)) {
    return std::nullopt;
  }

  auto relative = url.substr(prefix.size());
  auto end_pos =
      std::min(relative.find(static_cast<CharT>('?')), relative.find(static_cast<CharT>('#')));
  if (end_pos != std::basic_string_view<CharT>::npos) {
    relative = relative.substr(0, end_pos);
  }

  if (relative.empty()) {
    return std::nullopt;
  }

  return std::basic_string<CharT>(relative);
}

// 从完整 URL 中提取路径部分
auto extract_path_from_url(std::string_view url) -> std::optional<std::string> {
  auto scheme_pos = url.find("://");
  if (scheme_pos == std::string_view::npos) {
    return std::nullopt;
  }

  auto path_pos = url.find('/', scheme_pos + 3);
  if (path_pos == std::string_view::npos) {
    return std::string("/");
  }

  return std::string(url.substr(path_pos));
}

// 获取应用数据目录下的 backgrounds 文件夹路径
auto get_backgrounds_directory() -> std::expected<std::filesystem::path, std::string> {
  return Utils::Path::GetAppDataSubdirectory("backgrounds");
}

// 解析托管背景文件的绝对路径，进行安全校验
auto resolve_managed_background_file(const std::filesystem::path& file_name)
    -> std::expected<std::filesystem::path, std::string> {
  // 设置里只保存文件名，不保存路径；因此这里拒绝任何目录信息。
  if (file_name.empty() || file_name.is_absolute() || file_name.has_parent_path()) {
    return std::unexpected("Invalid managed background file name");
  }

  auto backgrounds_dir_result = get_backgrounds_directory();
  if (!backgrounds_dir_result) {
    return std::unexpected("Failed to get backgrounds directory: " +
                           backgrounds_dir_result.error());
  }

  auto resolved_path_result =
      Utils::Path::NormalizePath(backgrounds_dir_result.value() / file_name);
  if (!resolved_path_result) {
    return std::unexpected("Failed to resolve managed background file path: " +
                           resolved_path_result.error());
  }

  auto resolved_path = resolved_path_result.value();
  if (!is_path_within_base(resolved_path, backgrounds_dir_result.value())) {
    return std::unexpected("Managed background file escapes storage directory");
  }

  return resolved_path;
}

// 从原始路径解析托管背景文件路径
auto resolve_managed_background_file_name(std::string_view raw_file_name)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized = normalize_wallpaper_input_path(raw_file_name);
  if (normalized.empty()) {
    return std::unexpected("Managed background file name is empty");
  }
  return resolve_managed_background_file(std::filesystem::path(normalized));
}

// 确保背景文件存在且是普通文件
auto ensure_background_file_exists(const std::filesystem::path& path)
    -> std::expected<std::filesystem::path, std::string> {
  if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
    return std::unexpected("Background file not found");
  }
  return path;
}

// 解析并确保托管背景文件存在
auto resolve_existing_managed_background_file(const std::filesystem::path& relative_path)
    -> std::expected<std::filesystem::path, std::string> {
  auto path_result = resolve_managed_background_file(relative_path);
  if (!path_result) {
    return std::unexpected(path_result.error());
  }
  return ensure_background_file_exists(path_result.value());
}

// 生成唯一的背景文件名（时间戳 + 随机数）
auto generate_background_filename(const std::filesystem::path& source_path) -> std::string {
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  std::mt19937 random_engine(static_cast<std::mt19937::result_type>(std::random_device{}()));
  auto random_value = std::uniform_int_distribution<std::uint32_t>{}(random_engine);
  return std::format("{}-{:x}-{:08x}{}", kBackgroundFileName, timestamp, random_value,
                     normalize_background_extension(source_path));
}

// 将 RGB 颜色转换为十六进制字符串格式
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

// 将 RGB 颜色转换为 HSL 颜色空间
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

// 将 HSL 颜色转换回 RGB 颜色空间
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
    // 暗色 UI 上 accent 需更亮一些，否则从壁纸提取的主色容易偏闷
    l_min = primary ? 78.0 : 6.0;
    l_max = primary ? 90.0 : 18.0;
  }

  hsl.l = std::clamp(hsl.l, l_min, l_max);

  if (hsl.s < 12.0) {
    hsl.s = std::min(hsl.s, 20.0);
  } else {
    double s_max = saturation_cap_for_lightness(hsl.l);
    if (!primary) s_max *= 0.5;
    hsl.s = std::min(hsl.s, s_max);
  }

  return hsl_to_rgb(hsl);
}

// 解析壁纸路径，支持本地绝对路径和托管路径两种格式
auto resolve_background_file(std::string_view raw_file_name)
    -> std::expected<std::filesystem::path, std::string> {
  auto managed_file_result = resolve_managed_background_file_name(raw_file_name);
  if (!managed_file_result) {
    return std::unexpected(managed_file_result.error());
  }

  auto existing_path_result = ensure_background_file_exists(managed_file_result.value());
  if (!existing_path_result) {
    return std::unexpected("Background file does not exist: " +
                           managed_file_result.value().string());
  }

  return existing_path_result.value();
}

// 加载壁纸位图并缩放到分析尺寸，转换为 BGRA 格式
auto load_wallpaper_bitmap(const std::filesystem::path& path)
    -> std::expected<Utils::Image::BGRABitmapData, std::string> {
  auto wic_result = Utils::Image::get_thread_wic_factory();
  if (!wic_result) {
    return std::unexpected("Failed to initialize WIC factory: " + wic_result.error());
  }

  auto bitmap_data_result =
      Utils::Image::load_scaled_bgra_bitmap_data(wic_result->get(), path, kAnalysisSampleShortEdge);
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

// 从像素点集合计算平均颜色
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

// 在图像指定相对坐标位置采样区域主色
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

// 估算全图主色
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

// 根据亮度值判断主题模式（亮/暗）
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
auto analyze_background(const Types::BackgroundAnalysisParams& params)
    -> std::expected<Types::BackgroundAnalysisResult, std::string> {
  auto path_result = resolve_background_file(params.image_file_name);
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

  return Types::BackgroundAnalysisResult{
      .theme_mode = theme_mode,
      .primary_color = rgb_to_hex(compensated_primary),
      .overlay_colors = std::move(overlay_colors),
      .brightness = brightness,
  };
}

// 导入背景图片到应用托管目录
auto import_background_image(const Types::BackgroundImportParams& params)
    -> std::expected<Types::BackgroundImportResult, std::string> {
  try {
    // 导入阶段把用户原始文件复制到 app data 托管目录，
    // 设置里只保存逻辑路径，不直接保存物理文件系统路径。
    auto source_path_result = Utils::Path::NormalizePath(std::filesystem::path(params.source_path));
    if (!source_path_result) {
      return std::unexpected("Failed to resolve source background path: " +
                             source_path_result.error());
    }

    auto source_path = source_path_result.value();
    if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
      return std::unexpected("Background source file does not exist: " + source_path.string());
    }

    auto backgrounds_dir_result = get_backgrounds_directory();
    if (!backgrounds_dir_result) {
      return std::unexpected("Failed to get backgrounds directory: " +
                             backgrounds_dir_result.error());
    }

    auto destination_path =
        backgrounds_dir_result.value() / generate_background_filename(source_path);
    std::filesystem::copy_file(source_path, destination_path,
                               std::filesystem::copy_options::overwrite_existing);

    return Types::BackgroundImportResult{
        .image_file_name = destination_path.filename().generic_string(),
    };
  } catch (const std::exception& e) {
    return std::unexpected("Failed to import background image: " + std::string(e.what()));
  }
}

// 删除托管的背景图片
auto remove_background_image(const Types::BackgroundRemoveParams& params)
    -> std::expected<Types::BackgroundRemoveResult, std::string> {
  try {
    // 删除只接受托管路径，避免误删任意本地文件。
    auto resolved_path_result = resolve_managed_background_file_name(params.image_file_name);
    if (!resolved_path_result) {
      return std::unexpected(resolved_path_result.error());
    }

    bool removed = false;
    if (std::filesystem::exists(resolved_path_result.value())) {
      removed = std::filesystem::remove(resolved_path_result.value());
    }

    return Types::BackgroundRemoveResult{.removed = removed};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to remove background image: " + std::string(e.what()));
  }
}

// 注册背景图片的静态资源解析器（HTTP 和 WebView 两条链路）
auto register_static_resolvers(Core::State::AppState& app_state) -> void {
  // 开发环境下浏览器通过 HTTP 访问 /static/backgrounds/*，
  // 生产环境下 WebView 通过 static host 访问同一组逻辑路径。
  // 两条链路最终都落到 app data 的背景目录，不再依赖 resources/web。
  Core::HttpServer::Static::register_path_resolver(
      app_state, std::string(kBackgroundWebPrefix),
      [](std::string_view url) -> Core::HttpServer::Types::PathResolution {
        auto relative_path = extract_relative_path_generic(url, kBackgroundWebPrefix);
        if (!relative_path) {
          return std::unexpected("Invalid managed background URL");
        }

        auto background_path_result =
            resolve_existing_managed_background_file(std::filesystem::path(*relative_path));
        if (!background_path_result) {
          return std::unexpected(background_path_result.error());
        }

        return Core::HttpServer::Types::PathResolutionData{
            .file_path = background_path_result.value(),
            .cache_duration = std::chrono::hours(1),
        };
      });

  if (!app_state.webview) {
    return;
  }

  auto web_prefix = L"https://" + app_state.webview->config.static_host_name +
                    std::wstring(kBackgroundWebPrefixW);
  Core::WebView::Static::register_web_resource_resolver(
      app_state, web_prefix,
      [web_prefix](std::wstring_view url) -> Core::WebView::Types::WebResourceResolution {
        auto relative_path = extract_relative_path_generic(url, std::wstring_view(web_prefix));
        if (!relative_path) {
          return {.success = false,
                  .file_path = {},
                  .error_message = "Invalid managed background URL",
                  .content_type = std::nullopt,
                  .status_code = 404};
        }

        auto background_path_result =
            resolve_existing_managed_background_file(std::filesystem::path(*relative_path));
        if (!background_path_result) {
          return {.success = false,
                  .file_path = {},
                  .error_message = background_path_result.error(),
                  .content_type = std::nullopt,
                  .status_code = 404};
        }

        return {.success = true,
                .file_path = background_path_result.value(),
                .error_message = {},
                .content_type = std::nullopt,
                .status_code = 200};
      });

  Logger().info("Registered background static resolvers for {}", std::string(kBackgroundWebPrefix));
}

}  // namespace Features::Settings::Background
