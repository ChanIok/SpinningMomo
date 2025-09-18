module;

#include <windows.h>

export module Utils.Time;

import std;

namespace Utils::Time {

// 获取当前毫秒时间戳
export auto current_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// 文件时间转换为毫秒时间戳
export auto file_time_to_millis(const std::filesystem::file_time_type& file_time) -> std::int64_t {
  auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(file_time);
  return std::chrono::duration_cast<std::chrono::milliseconds>(system_time.time_since_epoch())
      .count();
}

// 获取文件创建时间的毫秒时间戳
export auto get_file_creation_time_millis(const std::filesystem::path& file_path)
    -> std::expected<std::int64_t, std::string> {
  WIN32_FILE_ATTRIBUTE_DATA fileAttr;

  if (!GetFileAttributesExW(file_path.c_str(), GetFileExInfoStandard, &fileAttr)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to get file attributes: {}", error));
  }

  // 转换创建时间
  FILETIME& creationTime = fileAttr.ftCreationTime;

  ULARGE_INTEGER ull;
  ull.LowPart = creationTime.dwLowDateTime;
  ull.HighPart = creationTime.dwHighDateTime;

  // Windows FILETIME 转 Unix 毫秒时间戳
  constexpr std::uint64_t EPOCH_DIFF = 116444736000000000ULL;  // 1970-1601 差值（100纳秒）
  const std::uint64_t unix_time_100ns = ull.QuadPart - EPOCH_DIFF;
  const std::uint64_t unix_time_millis = unix_time_100ns / 10000;  // 转换为毫秒

  return static_cast<std::int64_t>(unix_time_millis);
}

}  // namespace Utils::Time
