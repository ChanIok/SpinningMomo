module;

#include <asio.hpp>

export module Utils.File;

import std;

namespace Utils::File {

// 文件读取结果结构
export struct FileReadResult {
  std::string path;         // 文件路径
  std::string content;      // 文件内容（文本文件直接存储，二进制文件base64编码）
  std::string mime_type;    // MIME类型
  bool is_binary{false};    // 是否为二进制文件
  size_t original_size{0};  // 原始文件大小（字节）
};

// 文件写入结果结构
export struct FileWriteResult {
  std::string path;         // 文件路径
  size_t bytes_written{0};  // 实际写入字节数
};

// 目录项结构
export struct DirectoryEntry {
  std::string name;
  std::string path;
  std::string type;  // "file" or "directory"
  size_t size{0};    // 文件大小，目录为0
  std::string extension;
  int64_t last_modified;  // Unix 时间戳（毫秒）
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
  int64_t last_modified;  // Unix 时间戳（毫秒）
};

// 删除操作结果结构
export struct DeleteResult {
  size_t files_deleted{0};        // 删除的文件数量
  size_t directories_deleted{0};  // 删除的目录数量
  size_t total_bytes_freed{0};    // 释放的总字节数
  std::string path;               // 被删除的路径
};

// 移动/重命名操作结果结构
export struct MoveResult {
  std::string source_path;       // 源路径
  std::string destination_path;  // 目标路径
  bool was_renamed{false};       // 是否为重命名（同目录下）
  bool was_moved{false};         // 是否为移动（跨目录）
  size_t size{0};                // 移动的文件/目录大小
};

// 复制操作结果结构
export struct CopyResult {
  std::string source_path;        // 源路径
  std::string destination_path;   // 目标路径
  size_t files_copied{0};         // 复制的文件数量
  size_t directories_copied{0};   // 复制的目录数量
  size_t total_bytes_copied{0};   // 复制的总字节数
  bool is_recursive_copy{false};  // 是否为递归复制
};

// 异步读取文件
export auto read_file(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileReadResult, std::string>>;

// 异步写入文件（支持文本和二进制/base64解码）
export auto write_file(const std::filesystem::path &file_path, const std::string &content,
                       bool is_binary = false, bool overwrite = true)
    -> asio::awaitable<std::expected<FileWriteResult, std::string>>;

// 异步列出目录内容
export auto list_directory(const std::filesystem::path &dir_path,
                           const std::vector<std::string> &extensions = {})
    -> asio::awaitable<std::expected<DirectoryListResult, std::string>>;

// 异步获取文件信息
export auto get_file_info(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileInfoResult, std::string>>;

// 异步删除文件或目录
export auto delete_path(const std::filesystem::path &path, bool recursive = false)
    -> asio::awaitable<std::expected<DeleteResult, std::string>>;

// 异步移动或重命名文件/目录
export auto move_path(const std::filesystem::path &source_path,
                      const std::filesystem::path &destination_path, bool overwrite = false)
    -> asio::awaitable<std::expected<MoveResult, std::string>>;

// 异步复制文件或目录
export auto copy_path(const std::filesystem::path &source_path,
                      const std::filesystem::path &destination_path, bool recursive = false,
                      bool overwrite = false)
    -> asio::awaitable<std::expected<CopyResult, std::string>>;

}  // namespace Utils::File