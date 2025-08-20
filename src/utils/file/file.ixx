module;

#include <asio.hpp>

export module Utils.File;

import std;

namespace Utils::File {

// 文件读取结果结构
export struct FileReadResult {
  std::vector<char> content;  // 文件内容
  std::string mime_type;      // MIME类型
};

// 文件写入结果结构
export struct FileWriteResult {
  size_t bytes_written{0};  // 实际写入字节数
};

// 目录项结构
export struct DirectoryEntry {
  std::string name;
  std::string path;
  std::string type;  // "file" or "directory"
  size_t size{0};    // 文件大小，目录为0
  std::string extension;
  std::chrono::system_clock::time_point last_modified;
};

// 目录列表结果结构
export struct DirectoryListResult {
  std::vector<DirectoryEntry> directories;
  std::vector<DirectoryEntry> files;
  std::string path;
};

// 文件信息结果结构
export struct FileInfoResult {
  std::string path;
  bool exists{false};
  bool is_directory{false};
  bool is_regular_file{false};
  bool is_symlink{false};
  size_t size{0};
  std::string extension;
  std::string filename;
  std::chrono::system_clock::time_point last_modified;
};

// 异步读取文件
export auto read_file(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileReadResult, std::string>>;

// 异步写入文件
export auto write_file(const std::filesystem::path &file_path, const std::string &content,
                       bool overwrite = true)
    -> asio::awaitable<std::expected<FileWriteResult, std::string>>;

// 异步列出目录内容
export auto list_directory(const std::filesystem::path &dir_path,
                           const std::vector<std::string> &extensions = {})
    -> asio::awaitable<std::expected<DirectoryListResult, std::string>>;

// 异步获取文件信息
export auto get_file_info(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileInfoResult, std::string>>;

}  // namespace Utils::File