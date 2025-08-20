module;

export module Utils.File.Mime;

import std;

namespace Utils::File::Mime {

// 根据文件扩展名获取MIME类型，未知类型返回"application/octet-stream"
export auto get_mime_type_by_extension(const std::string &extension) -> std::string;

// 根据文件路径获取MIME类型，未知类型返回"application/octet-stream"
export auto get_mime_type(const std::filesystem::path &file_path) -> std::string;

// 根据文件路径字符串获取MIME类型，未知类型返回"application/octet-stream"
export auto get_mime_type(const std::string &file_path) -> std::string;

}  // namespace Utils::File::Mime