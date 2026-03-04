module;

module Features.Gallery.ScanCommon;

import std;
import Utils.String;
import Vendor.BuildConfig;
import Vendor.XXHash;

namespace Features::Gallery::ScanCommon {

auto default_supported_extensions() -> const std::vector<std::string>& {
  static const std::vector<std::string> kDefaultSupportedExtensions{
      ".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"};
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

auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  if (Vendor::BuildConfig::is_debug_build()) {
    auto path_str = file_path.string();
    auto hash = std::hash<std::string>{}(path_str);
    return std::format("{:016x}", hash);
  }

  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Cannot open file for hashing: " + file_path.string());
  }

  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  if (buffer.empty()) {
    return std::unexpected("File is empty: " + file_path.string());
  }

  return Vendor::XXHash::HashCharVectorToHex(buffer);
}

}  // namespace Features::Gallery::ScanCommon
