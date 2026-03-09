module;

module Plugins.InfinityNikki.PhotoExtract.Scan;

import std;
import Utils.String;

namespace Plugins::InfinityNikki::PhotoExtract::Scan {

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

auto extract_params_base64_from_file(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open photo file");
  }

  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 6) {
    return std::unexpected("Photo file is too small");
  }

  std::vector<std::size_t> eoi_positions;
  eoi_positions.reserve(4);
  for (std::size_t i = 0; i + 1 < data.size(); ++i) {
    if (data[i] == 0xFF && data[i + 1] == 0xD9) {
      eoi_positions.push_back(i);
    }
  }

  if (eoi_positions.size() < 2) {
    return std::unexpected("Not a dual-EOI photo");
  }

  auto p1 = eoi_positions[eoi_positions.size() - 2];
  auto p2 = eoi_positions[eoi_positions.size() - 1];
  if (p2 <= p1 + 2) {
    return std::unexpected("Invalid EOI boundaries");
  }

  std::string middle(reinterpret_cast<const char*>(data.data() + p1 + 2), p2 - (p1 + 2));
  middle.erase(std::remove_if(middle.begin(), middle.end(),
                              [](unsigned char ch) { return std::isspace(ch) != 0; }),
               middle.end());

  if (!is_base64_text(middle)) {
    return std::unexpected("Middle segment is not valid Base64 text");
  }

  return middle;
}

auto prepare_photo_extract_entry(const CandidateAssetRow& candidate)
    -> std::expected<PreparedPhotoExtractEntry, std::string> {
  auto uid_result = extract_uid_from_asset_path(candidate.path);
  if (!uid_result.has_value()) {
    return std::unexpected("cannot parse UID from path");
  }

  auto params_base64_result = extract_params_base64_from_file(to_filesystem_path(candidate.path));
  if (!params_base64_result) {
    return std::unexpected(params_base64_result.error());
  }

  return PreparedPhotoExtractEntry{
      .asset_id = candidate.id,
      .uid = std::move(uid_result.value()),
      .params_base64 = std::move(params_base64_result.value()),
  };
}

}  // namespace Plugins::InfinityNikki::PhotoExtract::Scan
