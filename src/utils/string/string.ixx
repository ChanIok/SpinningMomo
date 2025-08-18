module;

#include <windows.h>

#include <chrono>
#include <format>
#include <string>

export module Utils.String;

// 字符串工具命名空间
namespace Utils::String {

// 将宽字符串转换为UTF-8编码字符串
export [[nodiscard]] auto ToUtf8(const std::wstring& wide_str) noexcept -> std::string {
  if (wide_str.empty()) [[likely]]
    return {};

  const auto size_needed =
      WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()), nullptr,
                          0, nullptr, nullptr);

  if (size_needed <= 0) [[unlikely]]
    return {};

  std::string result(size_needed, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()),
                      result.data(), size_needed, nullptr, nullptr);

  return result;
}

// 将UTF-8编码字符串转换为宽字符串
export [[nodiscard]] auto FromUtf8(const std::string& utf8_str) noexcept -> std::wstring {
  if (utf8_str.empty()) [[likely]]
    return {};

  const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(),
                                               static_cast<int>(utf8_str.size()), nullptr, 0);

  if (size_needed <= 0) [[unlikely]]
    return {};

  std::wstring result(size_needed, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), static_cast<int>(utf8_str.size()),
                      result.data(), size_needed);

  return result;
}

// 格式化时间戳为文件名安全的字符串
export [[nodiscard]] auto FormatTimestamp(const std::chrono::system_clock::time_point& time_point)
    -> std::string {
  using namespace std::chrono;

  auto now = time_point;
  auto local_time = zoned_time{current_zone(), now};
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  return std::format("Screenshot_{:%Y%m%d_%H%M%S}_{:03d}.png", local_time, ms.count());
}

}  // namespace Utils::String