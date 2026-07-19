module;

module Features.Gallery.ScanCommon;

import std;
import Utils.String;
import Vendor.BuildConfig;
import Vendor.XXHash;

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

// 计算文件标识：Debug 使用路径哈希，Release 分块计算内容的 XXH3 哈希
auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  // Debug 保留轻量路径哈希，避免开发时为大文件读取完整内容
  if (Vendor::BuildConfig::is_debug_build()) {
    auto path_str = file_path.string();
    auto hash = std::hash<std::string>{}(path_str);
    return std::format("{:016x}", hash);
  }

  // Release 以二进制流打开文件，交给 XXH3 按固定缓冲区分块处理
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Cannot open file for hashing: " + file_path.string());
  }

  auto hash_result = Vendor::XXHash::hash_stream_to_hex(file);
  if (!hash_result) {
    return std::unexpected(
        std::format("Failed to hash file '{}': {}", file_path.string(), hash_result.error()));
  }

  return hash_result;
}

}  // namespace Features::Gallery::ScanCommon
