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

// Base64编码表
constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 将二进制数据编码为Base64字符串
export [[nodiscard]] auto ToBase64(const std::vector<char>& binary_data) -> std::string {
  std::string result;
  const auto data_size = binary_data.size();
  result.reserve((data_size + 2) / 3 * 4);

  for (size_t i = 0; i < data_size; i += 3) {
    const auto bytes_left = std::min<size_t>(3, data_size - i);

    uint32_t chunk = 0;
    for (size_t j = 0; j < bytes_left; ++j) {
      chunk |= (static_cast<uint8_t>(binary_data[i + j]) << (8 * (2 - j)));
    }

    result.push_back(kBase64Chars[(chunk >> 18) & 0x3F]);
    result.push_back(kBase64Chars[(chunk >> 12) & 0x3F]);
    result.push_back(bytes_left > 1 ? kBase64Chars[(chunk >> 6) & 0x3F] : '=');
    result.push_back(bytes_left > 2 ? kBase64Chars[chunk & 0x3F] : '=');
  }

  return result;
}

// 将Base64字符串解码为二进制数据
export [[nodiscard]] auto FromBase64(const std::string& base64_str) -> std::vector<char> {
  std::vector<char> result;
  if (base64_str.empty()) return result;

  // 创建解码表
  int decode_table[256];
  std::fill(std::begin(decode_table), std::end(decode_table), -1);
  for (int i = 0; i < 64; ++i) {
    decode_table[static_cast<uint8_t>(kBase64Chars[i])] = i;
  }

  const auto input_len = base64_str.length();
  result.reserve(input_len / 4 * 3);

  for (size_t i = 0; i < input_len; i += 4) {
    if (i + 3 >= input_len) break;

    const auto a = decode_table[static_cast<uint8_t>(base64_str[i])];
    const auto b = decode_table[static_cast<uint8_t>(base64_str[i + 1])];
    const auto c =
        base64_str[i + 2] == '=' ? -1 : decode_table[static_cast<uint8_t>(base64_str[i + 2])];
    const auto d =
        base64_str[i + 3] == '=' ? -1 : decode_table[static_cast<uint8_t>(base64_str[i + 3])];

    if (a == -1 || b == -1) break;

    result.push_back(static_cast<char>((a << 2) | (b >> 4)));

    if (c != -1) {
      result.push_back(static_cast<char>(((b & 0x0F) << 4) | (c >> 2)));

      if (d != -1) {
        result.push_back(static_cast<char>(((c & 0x03) << 6) | d));
      }
    }
  }

  return result;
}

// 检查字符串是否为有效的UTF-8
export [[nodiscard]] auto IsValidUtf8(const std::vector<char>& data) -> bool {
  for (size_t i = 0; i < data.size(); ++i) {
    const auto byte = static_cast<uint8_t>(data[i]);

    if (byte < 0x80) {
      // ASCII字符
      continue;
    } else if ((byte >> 5) == 0x06) {
      // 2字节UTF-8
      if (i + 1 >= data.size() || (static_cast<uint8_t>(data[i + 1]) >> 6) != 0x02) {
        return false;
      }
      i += 1;
    } else if ((byte >> 4) == 0x0E) {
      // 3字节UTF-8
      if (i + 2 >= data.size() || (static_cast<uint8_t>(data[i + 1]) >> 6) != 0x02 ||
          (static_cast<uint8_t>(data[i + 2]) >> 6) != 0x02) {
        return false;
      }
      i += 2;
    } else if ((byte >> 3) == 0x1E) {
      // 4字节UTF-8
      if (i + 3 >= data.size() || (static_cast<uint8_t>(data[i + 1]) >> 6) != 0x02 ||
          (static_cast<uint8_t>(data[i + 2]) >> 6) != 0x02 ||
          (static_cast<uint8_t>(data[i + 3]) >> 6) != 0x02) {
        return false;
      }
      i += 3;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace Utils::String