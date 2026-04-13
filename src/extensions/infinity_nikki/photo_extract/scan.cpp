module;

module Extensions.InfinityNikki.PhotoExtract.Scan;

import std;
import Utils.String;

namespace Extensions::InfinityNikki::PhotoExtract::Scan {

// 暖暖照片里真正要上传解析的数据，藏在 JPEG 尾部附近两个 EOI(FFD9) 之间。
// 所以这里不再整文件读入，而是优先从文件尾部倒着找这两个边界。
constexpr std::uint64_t kTailScanWindowBytes = 64 * 1024;

auto to_filesystem_path(const std::string& utf8_path) -> std::filesystem::path {
  return std::filesystem::path(Utils::String::FromUtf8(utf8_path));
}

auto normalize_path_for_matching(std::string path) -> std::string {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

auto extract_uid_from_asset_path(const std::string& asset_path) -> std::optional<std::string> {
  auto normalized = normalize_path_for_matching(asset_path);
  constexpr std::string_view kPrefix = "/X6Game/Saved/GamePlayPhotos/";

  auto prefix_pos = normalized.find(kPrefix);
  if (prefix_pos == std::string::npos) {
    return std::nullopt;
  }

  auto uid_start = prefix_pos + kPrefix.size();
  auto uid_end = normalized.find('/', uid_start);
  if (uid_end == std::string::npos || uid_end <= uid_start) {
    return std::nullopt;
  }

  return normalized.substr(uid_start, uid_end - uid_start);
}

auto is_base64_text(std::string_view text) -> bool {
  if (text.empty() || text.size() % 4 != 0) {
    return false;
  }

  for (unsigned char ch : text) {
    if (std::isalnum(ch) || ch == '+' || ch == '/' || ch == '=') {
      continue;
    }
    return false;
  }
  return true;
}

auto find_last_two_eoi_positions(std::ifstream& file, std::uint64_t file_size)
    -> std::expected<std::pair<std::uint64_t, std::uint64_t>, std::string> {
  if (file_size < 6) {
    return std::unexpected("Photo file is too small");
  }

  std::optional<std::uint64_t> last_eoi;
  std::optional<std::uint64_t> previous_eoi;
  std::uint64_t scan_end = file_size;

  while (scan_end > 0) {
    // 每次只读尾部一个窗口，逐步向前扩展，直到找到最后两个 FFD9。
    // 这样大多数照片都不需要把整张图搬进内存。
    auto read_start =
        scan_end > (kTailScanWindowBytes + 1) ? scan_end - (kTailScanWindowBytes + 1) : 0;
    auto chunk_size = scan_end - read_start;

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(chunk_size));

    file.clear();
    file.seekg(static_cast<std::streamoff>(read_start), std::ios::beg);
    if (!file) {
      return std::unexpected("Failed to seek photo file while scanning EOI markers");
    }

    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunk_size));
    if (file.gcount() != static_cast<std::streamsize>(chunk_size)) {
      return std::unexpected("Failed to read photo file while scanning EOI markers");
    }

    for (std::size_t i = buffer.size(); i > 1; --i) {
      auto pos = i - 2;
      if (buffer[pos] != 0xFF || buffer[pos + 1] != 0xD9) {
        continue;
      }

      auto global_pos = read_start + pos;
      if (!last_eoi.has_value()) {
        last_eoi = global_pos;
        continue;
      }

      if (global_pos == *last_eoi) {
        continue;
      }

      previous_eoi = global_pos;
      return std::pair{*previous_eoi, *last_eoi};
    }

    if (read_start == 0) {
      break;
    }

    scan_end = read_start + 1;
  }

  return std::unexpected("Not a dual-EOI photo");
}

auto extract_params_base64_from_file(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open photo file");
  }

  std::error_code file_size_error;
  auto file_size = std::filesystem::file_size(file_path, file_size_error);
  if (file_size_error) {
    return std::unexpected("Failed to get photo file size: " + file_size_error.message());
  }

  auto eoi_result = find_last_two_eoi_positions(file, file_size);
  if (!eoi_result) {
    return std::unexpected(eoi_result.error());
  }

  auto [p1, p2] = eoi_result.value();
  if (p2 <= p1 + 2) {
    return std::unexpected("Invalid EOI boundaries");
  }

  // 真正需要保留下来的只有这段中间文本。
  // 读取完之后再去掉空白并验证它看起来像合法 Base64。
  auto middle_size = p2 - (p1 + 2);
  if (middle_size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
    return std::unexpected("Embedded Base64 segment is too large");
  }

  std::string middle(static_cast<std::size_t>(middle_size), '\0');
  file.clear();
  file.seekg(static_cast<std::streamoff>(p1 + 2), std::ios::beg);
  if (!file) {
    return std::unexpected("Failed to seek embedded Base64 segment");
  }

  file.read(middle.data(), static_cast<std::streamsize>(middle.size()));
  if (file.gcount() != static_cast<std::streamsize>(middle.size())) {
    return std::unexpected("Failed to read embedded Base64 segment");
  }

  middle.erase(std::remove_if(middle.begin(), middle.end(),
                              [](unsigned char ch) { return std::isspace(ch) != 0; }),
               middle.end());

  if (!is_base64_text(middle)) {
    return std::unexpected("Middle segment is not valid Base64 text");
  }

  return middle;
}

auto prepare_photo_extract_entry(const CandidateAssetRow& candidate,
                                 const std::optional<std::string>& uid_override)
    -> std::expected<PreparedPhotoExtractEntry, std::string> {
  std::string uid;
  if (uid_override.has_value()) {
    if (uid_override->empty()) {
      return std::unexpected("UID override is empty");
    }
    uid = *uid_override;
  } else {
    auto uid_result = extract_uid_from_asset_path(candidate.path);
    if (!uid_result.has_value()) {
      return std::unexpected("cannot parse UID from path");
    }
    uid = std::move(uid_result.value());
  }

  auto params_base64_result = extract_params_base64_from_file(to_filesystem_path(candidate.path));
  if (!params_base64_result) {
    return std::unexpected(params_base64_result.error());
  }

  return PreparedPhotoExtractEntry{
      .asset_id = candidate.id,
      .uid = std::move(uid),
      .params_base64 = std::move(params_base64_result.value()),
  };
}

}  // namespace Extensions::InfinityNikki::PhotoExtract::Scan
