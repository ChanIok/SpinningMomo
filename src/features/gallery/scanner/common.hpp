#pragma once

namespace Features::Gallery::Scanner::Common {

auto default_supported_extensions() -> const std::vector<std::string>&;

auto is_supported_file(const std::filesystem::path& file_path,
                       const std::vector<std::string>& supported_extensions) -> bool;

auto is_photo_file(const std::filesystem::path& file_path) -> bool;

auto detect_asset_type(const std::filesystem::path& file_path) -> std::string;

// 计算素材内容指纹：Debug 使用路径哈希，Release 对小媒体完整哈希、对大媒体五点采样
auto calculate_content_fingerprint(const std::filesystem::path& file_path, std::int64_t file_size,
                                   std::stop_token stop_token)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery::Scanner::Common
