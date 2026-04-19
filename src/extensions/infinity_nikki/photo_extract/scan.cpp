module;

#include <windows.h>

#include <bcrypt.h>

module Extensions.InfinityNikki.PhotoExtract.Scan;

import std;
import Utils.String;

namespace Extensions::InfinityNikki::PhotoExtract::Scan {

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

auto trim_ascii_whitespace(const std::string& value) -> std::string {
  auto start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

auto read_file_bytes(const std::filesystem::path& file_path)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open photo file");
  }

  std::error_code file_size_error;
  auto file_size = std::filesystem::file_size(file_path, file_size_error);
  if (file_size_error) {
    return std::unexpected("Failed to get photo file size: " + file_size_error.message());
  }
  if (file_size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
    return std::unexpected("Photo file is too large");
  }

  std::vector<std::uint8_t> payload(static_cast<std::size_t>(file_size));
  file.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
  if (file.gcount() != static_cast<std::streamsize>(payload.size())) {
    return std::unexpected("Failed to read photo payload");
  }

  return payload;
}

auto find_roi(const std::vector<std::uint8_t>& payload, std::size_t cursor = 0) -> std::ptrdiff_t {
  if (payload.size() < 2) {
    return -1;
  }

  std::ptrdiff_t index =
      static_cast<std::ptrdiff_t>(cursor == 0 ? payload.size() : std::min(cursor, payload.size()));
  index -= 2;
  for (; index >= 0; --index) {
    if (payload[static_cast<std::size_t>(index)] == 0xFF &&
        payload[static_cast<std::size_t>(index + 1)] == 0xD9) {
      return index;
    }
  }
  return -1;
}

auto to_chars(const std::vector<std::uint8_t>& bytes) -> std::vector<char> {
  std::vector<char> chars;
  chars.reserve(bytes.size());
  for (auto byte : bytes) {
    chars.push_back(static_cast<char>(byte));
  }
  return chars;
}

auto is_nt_success(NTSTATUS status) -> bool { return status >= 0; }

auto make_ntstatus_error(std::string_view api_name, NTSTATUS status) -> std::string {
  return std::format("{} failed, NTSTATUS=0x{:08X}", api_name, static_cast<unsigned long>(status));
}

auto compute_md5_hex(const std::vector<std::uint8_t>& input, std::size_t length)
    -> std::expected<std::string, std::string> {
  if (length > input.size()) {
    return std::unexpected("MD5 input length exceeds payload size");
  }

  BCRYPT_ALG_HANDLE algorithm_handle = nullptr;
  BCRYPT_HASH_HANDLE hash_handle = nullptr;
  auto cleanup = [&]() {
    if (hash_handle != nullptr) {
      BCryptDestroyHash(hash_handle);
      hash_handle = nullptr;
    }
    if (algorithm_handle != nullptr) {
      BCryptCloseAlgorithmProvider(algorithm_handle, 0);
      algorithm_handle = nullptr;
    }
  };

  auto open_status =
      BCryptOpenAlgorithmProvider(&algorithm_handle, BCRYPT_MD5_ALGORITHM, nullptr, 0);
  if (!is_nt_success(open_status)) {
    cleanup();
    return std::unexpected(make_ntstatus_error("BCryptOpenAlgorithmProvider", open_status));
  }

  DWORD hash_object_size = 0;
  DWORD bytes_result = 0;
  auto object_size_status = BCryptGetProperty(algorithm_handle, BCRYPT_OBJECT_LENGTH,
                                              reinterpret_cast<PUCHAR>(&hash_object_size),
                                              sizeof(hash_object_size), &bytes_result, 0);
  if (!is_nt_success(object_size_status)) {
    cleanup();
    return std::unexpected(
        make_ntstatus_error("BCryptGetProperty(BCRYPT_OBJECT_LENGTH)", object_size_status));
  }

  DWORD hash_value_size = 0;
  auto hash_size_status = BCryptGetProperty(algorithm_handle, BCRYPT_HASH_LENGTH,
                                            reinterpret_cast<PUCHAR>(&hash_value_size),
                                            sizeof(hash_value_size), &bytes_result, 0);
  if (!is_nt_success(hash_size_status)) {
    cleanup();
    return std::unexpected(
        make_ntstatus_error("BCryptGetProperty(BCRYPT_HASH_LENGTH)", hash_size_status));
  }

  std::vector<UCHAR> hash_object(hash_object_size);
  std::vector<UCHAR> hash_value(hash_value_size);
  auto create_hash_status = BCryptCreateHash(algorithm_handle, &hash_handle, hash_object.data(),
                                             static_cast<ULONG>(hash_object.size()), nullptr, 0, 0);
  if (!is_nt_success(create_hash_status)) {
    cleanup();
    return std::unexpected(make_ntstatus_error("BCryptCreateHash", create_hash_status));
  }

  auto hash_data_status =
      BCryptHashData(hash_handle, const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(input.data())),
                     static_cast<ULONG>(length), 0);
  if (!is_nt_success(hash_data_status)) {
    cleanup();
    return std::unexpected(make_ntstatus_error("BCryptHashData", hash_data_status));
  }

  auto finish_status =
      BCryptFinishHash(hash_handle, hash_value.data(), static_cast<ULONG>(hash_value.size()), 0);
  if (!is_nt_success(finish_status)) {
    cleanup();
    return std::unexpected(make_ntstatus_error("BCryptFinishHash", finish_status));
  }

  cleanup();

  std::string hex;
  hex.reserve(hash_value.size() * 2);
  for (auto byte : hash_value) {
    hex += std::format("{:02x}", byte);
  }
  return hex;
}

auto build_photo_tuple(const std::vector<std::uint8_t>& payload, const std::string& uid)
    -> std::expected<std::pair<std::string, std::string>, std::string> {
  auto roi1 = find_roi(payload);
  if (roi1 < 0) {
    return std::unexpected("Missing ROI1 marker");
  }
  auto roi2 = find_roi(payload, static_cast<std::size_t>(roi1));
  if (roi2 < 0) {
    return std::unexpected("Missing ROI2 marker");
  }
  if (roi2 >= roi1) {
    return std::unexpected("Invalid ROI boundaries");
  }
  if (uid.empty()) {
    return std::unexpected("UID is empty");
  }

  auto bhash = trim_ascii_whitespace(std::string(
      reinterpret_cast<const char*>(payload.data() + roi1 + 2), payload.size() - (roi1 + 2)));
  auto bdata = trim_ascii_whitespace(
      std::string(reinterpret_cast<const char*>(payload.data() + roi2 + 2), (roi1 - 2) - roi2));

  if (!is_base64_text(bhash) || !is_base64_text(bdata)) {
    return std::unexpected("Embedded segments are not valid Base64 text");
  }

  auto hash_buf_chars = Utils::String::FromBase64(bhash);
  auto data_buf_chars = Utils::String::FromBase64(bdata);
  if (hash_buf_chars.empty() || data_buf_chars.empty()) {
    return std::unexpected("Failed to decode embedded Base64 segments");
  }

  std::vector<std::uint8_t> hash_buf;
  hash_buf.reserve(hash_buf_chars.size());
  for (char ch : hash_buf_chars) {
    hash_buf.push_back(static_cast<std::uint8_t>(ch));
  }
  std::vector<std::uint8_t> data_buf;
  data_buf.reserve(data_buf_chars.size());
  for (char ch : data_buf_chars) {
    data_buf.push_back(static_cast<std::uint8_t>(ch));
  }

  // decode-photo2：两 ROI Base64 解码后的字节直接拼接，不再做 XOR 混淆。
  std::vector<std::uint8_t> buf;
  buf.reserve(hash_buf.size() + data_buf.size());
  buf.insert(buf.end(), hash_buf.begin(), hash_buf.end());
  buf.insert(buf.end(), data_buf.begin(), data_buf.end());

  auto encoded = Utils::String::ToBase64(to_chars(buf));
  auto md5_result = compute_md5_hex(payload, static_cast<std::size_t>(roi1));
  if (!md5_result) {
    return std::unexpected(md5_result.error());
  }
  return std::pair{std::move(md5_result.value()), std::move(encoded)};
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

  auto payload_result = read_file_bytes(to_filesystem_path(candidate.path));
  if (!payload_result) {
    return std::unexpected(payload_result.error());
  }

  auto tuple_result = build_photo_tuple(payload_result.value(), uid);
  if (!tuple_result) {
    return std::unexpected(tuple_result.error());
  }

  return PreparedPhotoExtractEntry{
      .asset_id = candidate.id,
      .uid = std::move(uid),
      .md5 = std::move(tuple_result->first),
      .encoded = std::move(tuple_result->second),
  };
}

}  // namespace Extensions::InfinityNikki::PhotoExtract::Scan
