module;

export module Features.Gallery.ScanCommon;

import std;

namespace Features::Gallery::ScanCommon {

export auto default_supported_extensions() -> const std::vector<std::string>&;

export auto is_supported_file(const std::filesystem::path& file_path,
                              const std::vector<std::string>& supported_extensions) -> bool;

export auto is_photo_file(const std::filesystem::path& file_path) -> bool;

export auto detect_asset_type(const std::filesystem::path& file_path) -> std::string;

// 计算素材内容指纹：Debug 使用路径哈希，Release 对图片完整哈希、对大视频组合元数据与五点采样
export auto calculate_content_fingerprint(const std::filesystem::path& file_path,
                                          std::int64_t file_size, std::stop_token stop_token)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery::ScanCommon
