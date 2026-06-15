module;

#include <dkm.hpp>

module Utils.Image;

import std;
import Utils.Logger;
import <shlwapi.h>;
import <webp/encode.h>;
import <webp/types.h>;
import <wil/com.h>;
import <wil/result.h>;
import <wincodec.h>;
import <windows.h>;
import <winerror.h>;

namespace Utils::Image {

auto mix_kmeans_seed(std::uint64_t hash, std::uint64_t value) -> std::uint64_t {
  hash ^= value + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
  return hash;
}

auto build_kmeans_seed(const std::vector<std::array<float, 3>>& points, std::size_t cluster_count)
    -> std::uint64_t {
  std::uint64_t hash = 0xCBF29CE484222325ull;
  hash = mix_kmeans_seed(hash, static_cast<std::uint64_t>(points.size()));
  hash = mix_kmeans_seed(hash, static_cast<std::uint64_t>(cluster_count));

  for (const auto& point : points) {
    for (float component : point) {
      hash = mix_kmeans_seed(hash,
                             static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(component)));
    }
  }

  return hash == 0 ? 1 : hash;
}

// 用输入点集派生固定随机种子跑 K-Means，避免 dkm 默认 random_device 导致同图多次取色结果漂移
auto run_deterministic_kmeans(const std::vector<std::array<float, 3>>& points,
                              std::size_t cluster_count)
    -> std::expected<std::vector<std::size_t>, std::string> {
  if (points.empty()) {
    return std::unexpected("KMeans input has no points");
  }

  if (cluster_count == 0) {
    return std::unexpected("KMeans cluster count must be greater than zero");
  }

  auto effective_cluster_count = std::min(cluster_count, points.size());
  if (effective_cluster_count > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("KMeans cluster count is too large");
  }

  try {
    dkm::clustering_parameters<float> parameters(
        static_cast<std::uint32_t>(effective_cluster_count));
    parameters.set_random_seed(build_kmeans_seed(points, effective_cluster_count));

    auto [means, raw_labels] = dkm::kmeans_lloyd(points, parameters);
    // 最终颜色由簇内像素均值决定，簇心坐标仅用于分组
    (void)means;

    std::vector<std::size_t> labels;
    labels.reserve(raw_labels.size());
    for (const auto& label : raw_labels) {
      labels.push_back(static_cast<std::size_t>(label));
    }

    return labels;
  } catch (const std::exception& e) {
    return std::unexpected("Deterministic KMeans failed: " + std::string(e.what()));
  }
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

// 将 sRGB 字节值转换到 CIE Lab，供感知距离计算与 Lab 空间聚类使用
auto rgb_to_lab_color(std::uint8_t r, std::uint8_t g, std::uint8_t b) -> LabColor {
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

  return LabColor{
      .l = 116.0f * fy - 16.0f,
      .a = 500.0f * (fx - fy),
      .b = 200.0f * (fy - fz),
  };
}

// 从 BGRA 矩形区域采样 → Lab 聚类 → 按簇内像素均值生成权重排序的调色板
auto extract_lab_palette_from_bgra_rect(const BGRABitmapData& bitmap_data, int x0, int y0, int x1,
                                        int y1, const PaletteExtractOptions& options)
    -> std::expected<std::vector<PaletteColor>, std::string> {
  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  auto minimum_stride =
      static_cast<std::uint64_t>(bitmap_data.width) * static_cast<std::uint64_t>(4);
  if (bitmap_data.stride < minimum_stride) {
    return std::unexpected("Bitmap stride is smaller than width * 4");
  }

  auto required_bytes = static_cast<std::uint64_t>(bitmap_data.stride) *
                        static_cast<std::uint64_t>(bitmap_data.height);
  if (required_bytes > bitmap_data.pixels.size()) {
    return std::unexpected("Bitmap pixel buffer is smaller than stride * height");
  }

  int width = static_cast<int>(bitmap_data.width);
  int height = static_cast<int>(bitmap_data.height);
  x0 = std::clamp(x0, 0, width);
  y0 = std::clamp(y0, 0, height);
  x1 = std::clamp(x1, 0, width);
  y1 = std::clamp(y1, 0, height);
  if (x0 >= x1 || y0 >= y1) {
    return std::unexpected("Palette sample rect is empty");
  }

  struct SampledPalettePixel {
    RgbColor rgb;
    LabColor lab;
  };

  std::uint64_t area = static_cast<std::uint64_t>(x1 - x0) * static_cast<std::uint64_t>(y1 - y0);
  std::uint32_t max_samples = std::max<std::uint32_t>(1, options.max_samples);
  int pixel_step = 1;
  // 二维网格步进，在空间上均匀覆盖且总采样数不超过上限
  if (area > max_samples) {
    pixel_step = std::max(1, static_cast<int>(std::ceil(std::sqrt(
                                 static_cast<double>(area) / static_cast<double>(max_samples)))));
  }

  std::vector<SampledPalettePixel> samples;
  samples.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(area, max_samples)));
  for (int y = y0; y < y1; y += pixel_step) {
    for (int x = x0; x < x1; x += pixel_step) {
      std::uint64_t offset =
          static_cast<std::uint64_t>(y) * bitmap_data.stride + static_cast<std::uint64_t>(x) * 4;
      std::uint8_t b = bitmap_data.pixels[offset + 0];
      std::uint8_t g = bitmap_data.pixels[offset + 1];
      std::uint8_t r = bitmap_data.pixels[offset + 2];
      std::uint8_t a = bitmap_data.pixels[offset + 3];
      // 近透明像素不参与统计，避免透明区域拉偏主色
      if (a < options.min_alpha) continue;

      RgbColor rgb{.r = r, .g = g, .b = b};
      samples.push_back(SampledPalettePixel{
          .rgb = rgb,
          .lab = rgb_to_lab_color(rgb.r, rgb.g, rgb.b),
      });
    }
  }

  if (samples.empty()) {
    return std::unexpected("No valid pixels found for palette extraction");
  }

  std::vector<std::array<float, 3>> points;
  points.reserve(samples.size());
  for (const auto& sample : samples) {
    points.push_back({sample.lab.l, sample.lab.a, sample.lab.b});
  }

  auto cluster_count =
      std::clamp(static_cast<std::size_t>(options.cluster_count), std::size_t{1}, points.size());
  // 在 Lab 空间聚类，比 RGB 更符合人眼对"相近色"的感知
  auto labels_result = run_deterministic_kmeans(points, cluster_count);
  if (!labels_result) {
    return std::unexpected(labels_result.error());
  }

  struct ClusterAccumulator {
    std::size_t count = 0;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double lab_l = 0.0;
    double lab_a = 0.0;
    double lab_b = 0.0;
  };

  std::vector<ClusterAccumulator> clusters(cluster_count);
  for (std::size_t i = 0; i < samples.size() && i < labels_result->size(); ++i) {
    auto label = (*labels_result)[i];
    if (label >= clusters.size()) {
      continue;
    }

    auto& cluster = clusters[label];
    const auto& sample = samples[i];
    cluster.count += 1;
    cluster.r += sample.rgb.r;
    cluster.g += sample.rgb.g;
    cluster.b += sample.rgb.b;
    cluster.lab_l += sample.lab.l;
    cluster.lab_a += sample.lab.a;
    cluster.lab_b += sample.lab.b;
  }

  auto to_channel = [](double value, std::size_t count) -> std::uint8_t {
    long rounded = std::lround(value / static_cast<double>(count));
    return static_cast<std::uint8_t>(std::clamp(rounded, 0l, 255l));
  };

  std::vector<PaletteColor> palette;
  palette.reserve(clusters.size());
  const auto total_weight = static_cast<float>(samples.size());
  for (const auto& cluster : clusters) {
    if (cluster.count == 0) {
      continue;
    }

    auto count = static_cast<double>(cluster.count);
    palette.push_back(PaletteColor{
        .rgb =
            RgbColor{
                .r = to_channel(cluster.r, cluster.count),
                .g = to_channel(cluster.g, cluster.count),
                .b = to_channel(cluster.b, cluster.count),
            },
        .lab =
            LabColor{
                .l = static_cast<float>(cluster.lab_l / count),
                .a = static_cast<float>(cluster.lab_a / count),
                .b = static_cast<float>(cluster.lab_b / count),
            },
        .weight = static_cast<float>(cluster.count) / total_weight,
    });
  }

  // 权重降序，调用方取 front() 即画面占比最大的主色
  std::ranges::sort(palette, [](const PaletteColor& lhs, const PaletteColor& rhs) {
    if (lhs.weight != rhs.weight) return lhs.weight > rhs.weight;
    if (lhs.rgb.r != rhs.rgb.r) return lhs.rgb.r < rhs.rgb.r;
    if (lhs.rgb.g != rhs.rgb.g) return lhs.rgb.g < rhs.rgb.g;
    return lhs.rgb.b < rhs.rgb.b;
  });

  return palette;
}

// 格式化HRESULT错误信息
auto format_hresult(HRESULT hr, const std::string& context) -> std::string {
  char* message_buffer = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(hr), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

  std::string message;
  if (size > 0 && message_buffer) {
    message = std::format("{} (HRESULT: 0x{:08X}): {}", context, static_cast<unsigned int>(hr),
                          message_buffer);
    LocalFree(message_buffer);
  } else {
    message = std::format("{} (HRESULT: 0x{:08X})", context, static_cast<unsigned int>(hr));
  }
  return message;
}

// 获取MIME类型
auto get_mime_type(IWICBitmapDecoder* decoder) -> std::string {
  GUID container_format;
  if (FAILED(decoder->GetContainerFormat(&container_format))) {
    return "application/octet-stream";
  }

  if (IsEqualGUID(container_format, GUID_ContainerFormatBmp)) {
    return "image/bmp";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatPng)) {
    return "image/png";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatIco)) {
    return "image/x-icon";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatJpeg)) {
    return "image/jpeg";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatGif)) {
    return "image/gif";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatTiff)) {
    return "image/tiff";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatWmp)) {
    return "image/vnd.ms-photo";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatDds)) {
    return "image/vnd.ms-dds";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatAdng)) {
    return "image/apng";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatHeif)) {
    return "image/heif";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatWebp)) {
    return "image/webp";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatRaw)) {
    return "image/x-raw";
  }

  return "application/octet-stream";
}

// 计算缩放尺寸（按短边等比例缩放）
auto calculate_scaled_size(uint32_t original_width, uint32_t original_height,
                           uint32_t short_edge_size) -> std::pair<uint32_t, uint32_t> {
  // 判断哪边是短边
  uint32_t short_edge = std::min(original_width, original_height);

  // 如果短边已经小于或等于目标尺寸，不缩放
  if (short_edge <= short_edge_size) {
    return {original_width, original_height};
  }

  // 计算缩放比例（基于短边）
  double scale = static_cast<double>(short_edge_size) / short_edge;

  // 等比例计算两边
  uint32_t new_width = static_cast<uint32_t>(original_width * scale);
  uint32_t new_height = static_cast<uint32_t>(original_height * scale);

  // 确保至少为1像素
  new_width = std::max(new_width, 1u);
  new_height = std::max(new_height, 1u);

  return {new_width, new_height};
}

// 创建WIC工厂
auto create_factory() -> std::expected<WICFactory, std::string> {
  try {
    auto factory = wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory);
    return factory;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "Failed to create WIC factory"));
  }
}

// 获取当前线程的WIC工厂
auto get_thread_wic_factory() -> std::expected<WICFactory, std::string> {
  if (!thread_wic_factory) {
    if (!thread_com_init.has_value()) {
      thread_com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);
    }

    auto factory_result = create_factory();
    if (!factory_result) {
      return std::unexpected(factory_result.error());
    }
    thread_wic_factory = std::move(factory_result.value());
  }

  return thread_wic_factory;
}

// 获取图像信息
auto get_image_info(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<ImageInfo, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  try {
    // 创建解码器
    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));

    // 获取帧
    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

    // 获取尺寸
    UINT width, height;
    THROW_IF_FAILED(frame->GetSize(&width, &height));

    // 获取像素格式
    GUID pixel_format;
    THROW_IF_FAILED(frame->GetPixelFormat(&pixel_format));

    // 获取MIME类型
    auto mime_type = get_mime_type(decoder.get());

    return ImageInfo{static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixel_format,
                     mime_type};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 加载图像帧
auto load_bitmap_frame(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<wil::com_ptr<IWICBitmapFrameDecode>, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  try {
    // 创建解码器
    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));

    // 获取帧
    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

    return frame;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto copy_bgra_source_pixels(IWICBitmapSource* source)
    -> std::expected<BGRABitmapData, std::string> {
  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    UINT width = 0;
    UINT height = 0;
    THROW_IF_FAILED(source->GetSize(&width, &height));

    if (width == 0 || height == 0) {
      return std::unexpected("Bitmap source has empty dimensions");
    }

    if (width > static_cast<UINT>(std::numeric_limits<INT>::max()) ||
        height > static_cast<UINT>(std::numeric_limits<INT>::max())) {
      return std::unexpected("Bitmap source is too large to copy");
    }

    // 这里约定输出总是紧密排列的 32bpp BGRA，方便后续直接交给 libwebp。
    constexpr std::uint32_t kBgraBytesPerPixel = 4;
    auto stride_bytes = static_cast<std::uint64_t>(width) * kBgraBytesPerPixel;
    auto buffer_size = stride_bytes * height;
    if (stride_bytes > std::numeric_limits<UINT>::max() ||
        buffer_size > std::numeric_limits<UINT>::max()) {
      return std::unexpected("Bitmap source buffer is too large to copy");
    }

    BGRABitmapData result;
    result.width = static_cast<std::uint32_t>(width);
    result.height = static_cast<std::uint32_t>(height);
    result.stride = static_cast<std::uint32_t>(stride_bytes);
    result.pixels.resize(static_cast<std::size_t>(buffer_size));

    // CopyPixels 是这条流水线真正落到内存的地方；前面的 WIC source 仍可以保持惰性。
    WICRect rect = {0, 0, static_cast<INT>(width), static_cast<INT>(height)};
    THROW_IF_FAILED(source->CopyPixels(&rect, result.stride, static_cast<UINT>(buffer_size),
                                       result.pixels.data()));

    return result;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto convert_source_to_bgra_data(IWICImagingFactory* factory, IWICBitmapSource* source)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    // WIC 解码器输出的像素格式不固定，统一转成 BGRA 后再拷贝。
    wil::com_ptr<IWICFormatConverter> converter;
    THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
    THROW_IF_FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, nullptr, 0.0,
                                          WICBitmapPaletteTypeCustom));

    return copy_bgra_source_pixels(converter.get());
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory, IWICBitmapSource* source,
                                  uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    UINT original_width = 0;
    UINT original_height = 0;
    THROW_IF_FAILED(source->GetSize(&original_width, &original_height));

    auto [new_width, new_height] =
        calculate_scaled_size(original_width, original_height, short_edge_size);

    // 保持 frame -> scaler 紧邻。WIC 在 JPEG 等格式上可借此使用解码期降采样。
    IWICBitmapSource* scaled_source = source;
    wil::com_ptr<IWICBitmapScaler> scaler;
    if (new_width != original_width || new_height != original_height) {
      THROW_IF_FAILED(factory->CreateBitmapScaler(scaler.put()));
      THROW_IF_FAILED(
          scaler->Initialize(source, new_width, new_height, WICBitmapInterpolationModeFant));
      scaled_source = scaler.get();
    }

    return convert_source_to_bgra_data(factory, scaled_source);
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory, const std::filesystem::path& path,
                                  uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  // 文件入口只负责打开第一帧，真正的缩放/格式转换复用 source 入口。
  auto frame_result = load_bitmap_frame(factory, path);
  if (!frame_result) {
    return std::unexpected(frame_result.error());
  }

  return load_scaled_bgra_bitmap_data(factory, frame_result->get(), short_edge_size);
}

auto scale_bgra_bitmap_data(IWICImagingFactory* factory, const BGRABitmapData& bitmap_data,
                            uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  auto minimum_stride =
      static_cast<std::uint64_t>(bitmap_data.width) * static_cast<std::uint64_t>(4);
  if (bitmap_data.stride < minimum_stride) {
    return std::unexpected("Bitmap stride is smaller than width * 4");
  }

  auto required_bytes = static_cast<std::uint64_t>(bitmap_data.stride) *
                        static_cast<std::uint64_t>(bitmap_data.height);
  if (required_bytes > bitmap_data.pixels.size()) {
    return std::unexpected("Bitmap pixel buffer is smaller than stride * height");
  }

  if (bitmap_data.pixels.size() > std::numeric_limits<UINT>::max()) {
    return std::unexpected("Bitmap data is too large for WIC");
  }

  try {
    // 视频封面等来源已经是内存 BGRA，先包成 WIC source，再复用同一套缩放逻辑。
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(
        bitmap_data.width, bitmap_data.height, GUID_WICPixelFormat32bppBGRA, bitmap_data.stride,
        static_cast<UINT>(bitmap_data.pixels.size()), const_cast<BYTE*>(bitmap_data.pixels.data()),
        bitmap.put()));

    return load_scaled_bgra_bitmap_data(factory, bitmap.get(), short_edge_size);
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto encode_bgra_to_webp(const BGRABitmapData& bitmap_data, const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  if (bitmap_data.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      bitmap_data.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      bitmap_data.stride > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    return std::unexpected("Bitmap data dimensions are too large for WebP");
  }

  auto minimum_stride =
      static_cast<std::uint64_t>(bitmap_data.width) * static_cast<std::uint64_t>(4);
  if (bitmap_data.stride < minimum_stride) {
    return std::unexpected("Bitmap stride is smaller than width * 4");
  }

  auto required_bytes = static_cast<std::uint64_t>(bitmap_data.stride) *
                        static_cast<std::uint64_t>(bitmap_data.height);
  if (required_bytes > bitmap_data.pixels.size()) {
    return std::unexpected("Bitmap pixel buffer is smaller than stride * height");
  }

  try {
    // libwebp 只需要 BGRA 指针、宽高和 stride；不再额外创建 WIC bitmap。
    uint8_t* output = nullptr;
    size_t output_size = 0;
    auto width = static_cast<int>(bitmap_data.width);
    auto height = static_cast<int>(bitmap_data.height);
    auto stride = static_cast<int>(bitmap_data.stride);

    if (options.lossless) {
      output_size =
          WebPEncodeLosslessBGRA(bitmap_data.pixels.data(), width, height, stride, &output);
    } else {
      output_size = WebPEncodeBGRA(bitmap_data.pixels.data(), width, height, stride,
                                   options.quality, &output);
    }

    if (output_size == 0 || output == nullptr) {
      return std::unexpected("Failed to encode WebP image");
    }

    // libwebp 分配输出内存；复制到 vector 后必须用 WebPFree 释放。
    std::vector<uint8_t> result_data(output, output + output_size);
    WebPFree(output);

    return WebPEncodedResult{std::move(result_data), bitmap_data.width, bitmap_data.height};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 视频封面：MF 已解码为 RGB32/BGRA 内存帧，无需落盘即可走与照片相同的缩放 + WebP 编码。
auto generate_webp_thumbnail_from_bgra(IWICImagingFactory* factory,
                                       const BGRABitmapData& bitmap_data, uint32_t short_edge_size,
                                       const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  auto scaled_result = scale_bgra_bitmap_data(factory, bitmap_data, short_edge_size);
  if (!scaled_result) {
    return std::unexpected(scaled_result.error());
  }

  return encode_bgra_to_webp(scaled_result.value(), options);
}

auto read_stream_bytes(IStream* stream) -> std::expected<std::vector<uint8_t>, std::string> {
  if (!stream) {
    return std::unexpected("Stream is null");
  }

  try {
    // 先用 Stat 取最终长度，再一次性读到 vector。
    // 这里的 stream 既可能来自普通文件编码，也可能来自内存 JPEG 编码。
    STATSTG stat{};
    THROW_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));

    if (stat.cbSize.QuadPart < 0 || static_cast<std::uint64_t>(stat.cbSize.QuadPart) >
                                        std::numeric_limits<std::size_t>::max()) {
      return std::unexpected("Encoded stream is too large");
    }

    LARGE_INTEGER seek_origin{};
    THROW_IF_FAILED(stream->Seek(seek_origin, STREAM_SEEK_SET, nullptr));

    std::vector<uint8_t> bytes(static_cast<std::size_t>(stat.cbSize.QuadPart));
    if (!bytes.empty()) {
      ULONG bytes_read = 0;
      THROW_IF_FAILED(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &bytes_read));
      if (bytes_read != bytes.size()) {
        return std::unexpected("Failed to read complete encoded stream");
      }
    }

    return bytes;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "Failed to read encoded stream"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto encode_pixel_data_to_jpeg_bytes(IWICImagingFactory* factory, const uint8_t* pixel_data,
                                     uint32_t width, uint32_t height, uint32_t row_pitch,
                                     const GUID& source_pixel_format, float jpeg_quality)
    -> std::expected<std::vector<uint8_t>, std::string> {
  // 这是新加的“内存 JPEG 编码”公共实现：
  // - 不直接写文件
  // - 把编码结果留在内存里，供上层继续 mux / 拼容器
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!pixel_data) {
    return std::unexpected("Pixel data is null");
  }

  if (width == 0 || height == 0 || row_pitch == 0) {
    return std::unexpected("Pixel data dimensions are invalid");
  }

  try {
    // 用 HGLOBAL-backed IStream 接住 WIC 输出，这样调用方能直接拿到 JPEG bytes。
    wil::com_ptr<IStream> stream;
    THROW_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.put()));

    wil::com_ptr<IWICBitmapEncoder> encoder;
    THROW_IF_FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, encoder.put()));
    THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> property_bag;
    THROW_IF_FAILED(encoder->CreateNewFrame(frame.put(), property_bag.put()));

    if (property_bag) {
      // WIC JPEG 质量参数是 0..1 浮点，不是 0..100。
      PROPBAG2 quality_option = {};
      quality_option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
      VARIANT quality_value;
      VariantInit(&quality_value);
      quality_value.vt = VT_R4;
      quality_value.fltVal = std::clamp(jpeg_quality, 0.0f, 1.0f);
      property_bag->Write(1, &quality_option, &quality_value);
    }

    THROW_IF_FAILED(frame->Initialize(property_bag.get()));
    THROW_IF_FAILED(frame->SetSize(width, height));

    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(width, height, source_pixel_format, row_pitch,
                                                    row_pitch * height,
                                                    const_cast<BYTE*>(pixel_data), bitmap.put()));

    // 这里总是把源像素如实声明给 WIC，再让 JPEG 编码器做必要的内部转换。
    // Ultra HDR 截图的 base 与 gain map 均使用 32bppBGRA。
    WICPixelFormatGUID pixel_format = source_pixel_format;
    THROW_IF_FAILED(frame->SetPixelFormat(&pixel_format));
    THROW_IF_FAILED(frame->WriteSource(bitmap.get(), nullptr));
    THROW_IF_FAILED(frame->Commit());
    THROW_IF_FAILED(encoder->Commit());

    return read_stream_bytes(stream.get());
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC JPEG encode failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto encode_bgra_to_jpeg_bytes(IWICImagingFactory* factory, const uint8_t* pixel_data,
                               uint32_t width, uint32_t height, uint32_t row_pitch,
                               float jpeg_quality)
    -> std::expected<std::vector<uint8_t>, std::string> {
  // SDR base 与 Ultra HDR gain map 走这一层。
  return encode_pixel_data_to_jpeg_bytes(factory, pixel_data, width, height, row_pitch,
                                         GUID_WICPixelFormat32bppBGRA, jpeg_quality);
}

// 保存像素数据到文件
auto save_pixel_data_to_file(IWICImagingFactory* factory, const uint8_t* pixel_data, uint32_t width,
                             uint32_t height, uint32_t row_pitch, const std::wstring& file_path,
                             ImageFormat format, float jpeg_quality)
    -> std::expected<void, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!pixel_data) {
    return std::unexpected("Pixel data is null");
  }

  try {
    // 根据格式选择编码器 GUID
    GUID container_format =
        (format == ImageFormat::JPEG) ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng;

    wil::com_ptr<IWICBitmapEncoder> encoder;
    THROW_IF_FAILED(factory->CreateEncoder(container_format, nullptr, encoder.put()));

    // 创建流
    wil::com_ptr<IWICStream> stream;
    THROW_IF_FAILED(factory->CreateStream(stream.put()));
    THROW_IF_FAILED(stream->InitializeFromFilename(file_path.c_str(), GENERIC_WRITE));

    // 初始化编码器
    THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    // 创建新帧（带属性包用于 JPEG 质量设置）
    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> property_bag;
    THROW_IF_FAILED(encoder->CreateNewFrame(frame.put(), property_bag.put()));

    // JPEG 格式时设置质量参数
    if (format == ImageFormat::JPEG && property_bag) {
      PROPBAG2 quality_option = {};
      quality_option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
      VARIANT quality_value;
      VariantInit(&quality_value);
      quality_value.vt = VT_R4;
      quality_value.fltVal = jpeg_quality;
      property_bag->Write(1, &quality_option, &quality_value);
    }

    // 初始化帧
    THROW_IF_FAILED(frame->Initialize(property_bag.get()));

    // 设置帧尺寸
    THROW_IF_FAILED(frame->SetSize(width, height));

    // 用原始像素数据创建 WIC 位图（声明为 32bppBGRA）
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA,
                                                    row_pitch, row_pitch * height,
                                                    const_cast<BYTE*>(pixel_data), bitmap.put()));

    // 设置像素格式（让编码器协商：JPEG→24bppBGR, PNG→32bppBGRA）
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    THROW_IF_FAILED(frame->SetPixelFormat(&pixel_format));

    // WriteSource 自动将 32bppBGRA 转换为编码器协商的格式
    THROW_IF_FAILED(frame->WriteSource(bitmap.get(), nullptr));

    // 提交帧
    THROW_IF_FAILED(frame->Commit());

    // 提交编码器
    THROW_IF_FAILED(encoder->Commit());

    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}
}  // namespace Utils::Image
