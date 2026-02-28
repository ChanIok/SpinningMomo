module;

#include <windows.h>

#include <bcrypt.h>

module Utils.Crypto;

import std;

namespace Utils::Crypto {

auto is_nt_success(NTSTATUS status) -> bool { return status >= 0; }

auto make_ntstatus_error(std::string_view api_name, NTSTATUS status) -> std::string {
  return std::format("{} failed, NTSTATUS=0x{:08X}", api_name, static_cast<unsigned long>(status));
}

auto sha256_file(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
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
      BCryptOpenAlgorithmProvider(&algorithm_handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
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

  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    cleanup();
    return std::unexpected("Failed to open file for hashing: " + file_path.string());
  }

  std::array<char, 64 * 1024> buffer{};
  while (file.good()) {
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    auto bytes_read = file.gcount();
    if (bytes_read <= 0) {
      break;
    }

    auto hash_data_status = BCryptHashData(hash_handle, reinterpret_cast<PUCHAR>(buffer.data()),
                                           static_cast<ULONG>(bytes_read), 0);
    if (!is_nt_success(hash_data_status)) {
      cleanup();
      return std::unexpected(make_ntstatus_error("BCryptHashData", hash_data_status));
    }
  }

  if (file.bad()) {
    cleanup();
    return std::unexpected("Failed while reading file for hashing: " + file_path.string());
  }

  auto finish_status =
      BCryptFinishHash(hash_handle, hash_value.data(), static_cast<ULONG>(hash_value.size()), 0);
  if (!is_nt_success(finish_status)) {
    cleanup();
    return std::unexpected(make_ntstatus_error("BCryptFinishHash", finish_status));
  }

  cleanup();

  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (auto byte : hash_value) {
    output << std::setw(2) << static_cast<int>(byte);
  }
  return output.str();
}

}  // namespace Utils::Crypto
