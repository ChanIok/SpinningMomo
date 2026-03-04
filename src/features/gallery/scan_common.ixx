module;

export module Features.Gallery.ScanCommon;

import std;

namespace Features::Gallery::ScanCommon {

export auto default_supported_extensions() -> const std::vector<std::string>&;

export auto is_supported_file(const std::filesystem::path& file_path,
                              const std::vector<std::string>& supported_extensions) -> bool;

export auto is_photo_file(const std::filesystem::path& file_path) -> bool;

export auto detect_asset_type(const std::filesystem::path& file_path) -> std::string;

export auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery::ScanCommon
