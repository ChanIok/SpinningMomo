module;

#include <windows.h>

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

}  // namespace Utils::String