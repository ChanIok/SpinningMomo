#pragma once

namespace Utils::Crypto {

// 计算文件 SHA-256（小写十六进制字符串）
auto sha256_file(const std::filesystem::path& file_path) -> std::expected<std::string, std::string>;

}  // namespace Utils::Crypto
