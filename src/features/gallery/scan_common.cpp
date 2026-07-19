module;

module Features.Gallery.ScanCommon;

import std;
import Vendor.BuildConfig;
import Vendor.XXHash;
import Utils.Media.VideoAsset;
import Utils.String;

namespace Features::Gallery::ScanCommon {

auto default_supported_extensions() -> const std::vector<std::string>& {
  // 与 web GalleryScanDialog 默认列表、RPC 扫描选项保持一致，避免前后端可扫范围不一致。
  static const std::vector<std::string> kDefaultSupportedExtensions{
      ".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif",
      ".mp4", ".avi",  ".mov", ".mkv", ".wmv",  ".webm"};
  return kDefaultSupportedExtensions;
}

auto lower_extension(const std::filesystem::path& file_path) -> std::string {
  if (!file_path.has_extension()) {
    return {};
  }
  return Utils::String::ToLowerAscii(file_path.extension().string());
}

auto is_photo_extension(const std::string& extension) -> bool {
  return extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
         extension == ".bmp" || extension == ".webp" || extension == ".tiff" || extension == ".tif";
}

auto is_video_extension(const std::string& extension) -> bool {
  return extension == ".mp4" || extension == ".avi" || extension == ".mov" || extension == ".mkv" ||
         extension == ".wmv" || extension == ".webm";
}

auto is_supported_file(const std::filesystem::path& file_path,
                       const std::vector<std::string>& supported_extensions) -> bool {
  auto extension = lower_extension(file_path);
  if (extension.empty()) {
    return false;
  }

  return std::ranges::find(supported_extensions, extension) != supported_extensions.end();
}

auto is_photo_file(const std::filesystem::path& file_path) -> bool {
  return is_photo_extension(lower_extension(file_path));
}

auto detect_asset_type(const std::filesystem::path& file_path) -> std::string {
  auto extension = lower_extension(file_path);
  if (extension.empty()) {
    return "unknown";
  }

  if (is_photo_extension(extension)) {
    return "photo";
  }

  if (is_video_extension(extension)) {
    return "video";
  }

  return "unknown";
}

constexpr std::uint64_t kVideoSampleSize = 1024 * 1024;
constexpr std::size_t kVideoSampleCount = 5;
constexpr std::uint64_t kVideoFullHashThreshold = kVideoSampleSize * kVideoSampleCount;

// 为大视频计算“稳定媒体元数据 + 五个均匀采样块”的内容指纹
auto calculate_sampled_video_fingerprint(const std::filesystem::path& file_path,
                                         std::uint64_t file_size, std::ifstream& file,
                                         std::stop_token stop_token)
    -> std::expected<std::string, std::string> {
  // 元数据解析失败时保留零值，采样内容仍可为该文件提供稳定指纹。
  std::uint64_t width = 0;
  std::uint64_t height = 0;
  std::uint64_t duration_millis = 0;
  auto video_result = Utils::Media::VideoAsset::analyze_video_file(file_path, std::nullopt);
  if (video_result) {
    width = video_result->width;
    height = video_result->height;
    duration_millis = static_cast<std::uint64_t>(video_result->duration_millis.value_or(0));
  }

  const std::array<std::uint64_t, 4> metadata{file_size, width, height, duration_millis};
  auto metadata_bytes = std::as_bytes(std::span{metadata});

  // 以合法起点范围均分五处，确保最后一个采样块正好覆盖文件尾部。
  auto last_offset = file_size - kVideoSampleSize;
  const std::array<Vendor::XXHash::StreamRange, kVideoSampleCount> ranges{
      Vendor::XXHash::StreamRange{.offset = 0, .size = kVideoSampleSize},
      Vendor::XXHash::StreamRange{.offset = last_offset / 4, .size = kVideoSampleSize},
      Vendor::XXHash::StreamRange{.offset = last_offset / 2, .size = kVideoSampleSize},
      Vendor::XXHash::StreamRange{.offset = last_offset - last_offset / 4,
                                  .size = kVideoSampleSize},
      Vendor::XXHash::StreamRange{.offset = last_offset, .size = kVideoSampleSize},
  };

  return Vendor::XXHash::hash_stream_ranges_to_hex(file, metadata_bytes, ranges, stop_token);
}

// 计算素材内容指纹：Debug 使用路径哈希，Release 对图片完整哈希、对大视频组合元数据与五点采样
auto calculate_content_fingerprint(const std::filesystem::path& file_path, std::int64_t file_size,
                                   std::stop_token stop_token)
    -> std::expected<std::string, std::string> {
  // 停止后不再打开文件，避免退出阶段继续产生磁盘访问
  if (stop_token.stop_requested()) {
    return std::unexpected("Hash calculation cancelled");
  }

  // Debug 保留轻量路径哈希，避免开发时为大文件读取完整内容
  if (Vendor::BuildConfig::is_debug_build()) {
    auto path_str = file_path.string();
    auto hash = std::hash<std::string>{}(path_str);
    return std::format("{:016x}", hash);
  }

  if (file_size < 0) {
    return std::unexpected("Cannot fingerprint a file with negative size: " + file_path.string());
  }

  // Release 以二进制流打开文件，小文件和图片继续使用完整内容哈希。
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Cannot open file for fingerprinting: " + file_path.string());
  }

  std::expected<std::string, std::string> hash_result;
  auto unsigned_size = static_cast<std::uint64_t>(file_size);
  if (detect_asset_type(file_path) == "video" && unsigned_size > kVideoFullHashThreshold) {
    // 大视频只读取五个 1 MiB 区间，避免素材导入量随视频总大小线性增长。
    hash_result = calculate_sampled_video_fingerprint(file_path, unsigned_size, file, stop_token);
  } else {
    // 小于等于 5 MiB 的视频直接完整哈希，避免采样区间重叠规则。
    hash_result = Vendor::XXHash::hash_stream_to_hex(file, stop_token);
  }

  if (!hash_result) {
    return std::unexpected(std::format("Failed to fingerprint file '{}': {}", file_path.string(),
                                       hash_result.error()));
  }

  return hash_result;
}

}  // namespace Features::Gallery::ScanCommon
