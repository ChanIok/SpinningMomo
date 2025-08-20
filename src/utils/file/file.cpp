module;

#include <asio.hpp>

module Utils.File;

import std;
import Utils.Logger;
import Utils.File.Mime;
import Utils.String;

namespace Utils::File {

auto to_timestamp(std::chrono::system_clock::time_point tp) -> int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

auto format_file_error(const std::string &operation, const std::filesystem::path &path,
                       const std::exception &e) -> std::string {
  std::string error_msg = std::format("{} {}: {}", operation, path.string(), e.what());
  Logger().error("{} {}: {}", operation, path.string(), e.what());
  return error_msg;
}

// 判断MIME类型是否为文本类型
auto is_text_mime_type(const std::string &mime_type) -> bool {
  return mime_type.starts_with("text/") || mime_type.starts_with("application/json") ||
         mime_type.starts_with("application/xml") ||
         mime_type.starts_with("application/javascript") || mime_type.ends_with("; charset=utf-8");
}

auto read_file(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileReadResult, std::string>> {
  try {
    // 获取当前executor
    auto executor = co_await asio::this_coro::executor;

    asio::stream_file file(executor, file_path.string(), asio::file_base::read_only);
    auto file_size = file.size();

    FileReadResult result;
    result.mime_type = Mime::get_mime_type(file_path);
    result.original_size = file_size;

    if (file_size == 0) {
      co_return result;
    }

    // 先读取为二进制数据
    std::vector<char> binary_data(file_size);
    auto bytes_read =
        co_await asio::async_read(file, asio::buffer(binary_data), asio::use_awaitable);

    // 调整实际读取大小
    if (bytes_read < file_size) {
      binary_data.resize(bytes_read);
      result.original_size = bytes_read;
    }

    // 判断文件类型并处理内容
    bool is_text = is_text_mime_type(result.mime_type);
    if (!is_text) {
      // 对于未知类型，尝试UTF-8验证
      is_text = Utils::String::IsValidUtf8(binary_data);
    }

    if (is_text) {
      // 文本文件：直接转换为字符串
      result.content = std::string(binary_data.begin(), binary_data.end());
      result.is_binary = false;
      Logger().debug("Read text file: {}, size: {} chars, mime: {}", file_path.string(),
                     result.content.size(), result.mime_type);
    } else {
      // 二进制文件：Base64编码
      result.content = Utils::String::ToBase64(binary_data);
      result.is_binary = true;
      Logger().debug(
          "Read binary file: {}, original size: {} bytes, encoded size: {} chars, mime: {}",
          file_path.string(), result.original_size, result.content.size(), result.mime_type);
    }

    co_return result;
  } catch (const std::exception &e) {
    co_return std::unexpected(format_file_error("Error reading", file_path, e));
  }
}

auto write_file(const std::filesystem::path &file_path, const std::string &content, bool is_binary,
                bool overwrite) -> asio::awaitable<std::expected<FileWriteResult, std::string>> {
  try {
    auto executor = co_await asio::this_coro::executor;

    // 确保父目录存在
    auto parent_path = file_path.parent_path();
    if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
      std::filesystem::create_directories(parent_path);
      Logger().debug("Created directories: {}", parent_path.string());
    }

    // 检查文件是否存在且不允许覆盖
    if (std::filesystem::exists(file_path) && !overwrite) {
      co_return std::unexpected("File already exists and overwrite is disabled: " +
                                file_path.string());
    }

    asio::stream_file file(
        executor, file_path.string(),
        asio::file_base::write_only | asio::file_base::create | asio::file_base::truncate);

    if (content.empty()) {
      co_return FileWriteResult{};
    }

    size_t bytes_written = 0;
    if (is_binary) {
      // 二进制文件：从base64解码后写入
      auto binary_data = Utils::String::FromBase64(content);
      if (!binary_data.empty()) {
        bytes_written =
            co_await asio::async_write(file, asio::buffer(binary_data), asio::use_awaitable);
      }
      Logger().debug("Successfully wrote binary file: {}, decoded size: {} bytes",
                     file_path.string(), bytes_written);
    } else {
      // 文本文件：直接写入字符串
      bytes_written = co_await asio::async_write(file, asio::buffer(content), asio::use_awaitable);
      Logger().debug("Successfully wrote text file: {}, size: {} bytes", file_path.string(),
                     bytes_written);
    }

    FileWriteResult result{.bytes_written = bytes_written};
    co_return result;
  } catch (const std::exception &e) {
    co_return std::unexpected(format_file_error("Error writing", file_path, e));
  }
}

auto list_directory(const std::filesystem::path &dir_path,
                    const std::vector<std::string> &extensions)
    -> asio::awaitable<std::expected<DirectoryListResult, std::string>> {
  try {
    if (!std::filesystem::exists(dir_path)) {
      co_return std::unexpected("Directory not found: " + dir_path.string());
    }

    if (!std::filesystem::is_directory(dir_path)) {
      co_return std::unexpected("Path is not a directory: " + dir_path.string());
    }

    DirectoryListResult result{.path = dir_path.string()};

    // 创建扩展名过滤函数
    auto should_include_file = [&extensions](const std::string &file_ext) {
      if (extensions.empty()) return true;
      return std::ranges::any_of(extensions,
                                 [&file_ext](const auto &ext) { return file_ext == ext; });
    };

    // 遍历目录
    for (const auto &entry : std::filesystem::directory_iterator(dir_path)) {
      DirectoryEntry dir_entry{.name = entry.path().filename().string(),
                               .path = entry.path().string()};

      auto last_write_time = std::filesystem::last_write_time(entry.path());
      dir_entry.last_modified =
          to_timestamp(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              last_write_time - std::filesystem::file_time_type::clock::now() +
              std::chrono::system_clock::now()));

      if (entry.is_directory()) {
        dir_entry.type = "directory";
        result.directories.push_back(std::move(dir_entry));
      } else if (entry.is_regular_file()) {
        std::string file_ext = entry.path().extension().string();
        if (should_include_file(file_ext)) {
          dir_entry.type = "file";
          dir_entry.size = entry.file_size();
          dir_entry.extension = file_ext;
          result.files.push_back(std::move(dir_entry));
        }
      }
    }

    Logger().debug("Successfully listed directory: {}, {} directories, {} files", dir_path.string(),
                   result.directories.size(), result.files.size());
    co_return result;
  } catch (const std::exception &e) {
    co_return std::unexpected(format_file_error("Error listing directory", dir_path, e));
  }
}

auto get_file_info(const std::filesystem::path &file_path)
    -> asio::awaitable<std::expected<FileInfoResult, std::string>> {
  try {
    if (!std::filesystem::exists(file_path)) {
      co_return FileInfoResult{.path = file_path.string(), .exists = false};
    }

    std::filesystem::file_status status = std::filesystem::status(file_path);

    FileInfoResult result{.path = file_path.string(),
                          .exists = true,
                          .is_directory = std::filesystem::is_directory(status),
                          .is_regular_file = std::filesystem::is_regular_file(status),
                          .is_symlink = std::filesystem::is_symlink(status)};

    if (result.is_regular_file) {
      result.size = std::filesystem::file_size(file_path);
      result.extension = file_path.extension().string();
      result.filename = file_path.filename().string();
    }

    auto last_write_time = std::filesystem::last_write_time(file_path);
    result.last_modified =
        to_timestamp(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            last_write_time - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now()));

    Logger().debug("Successfully got file info: {}", file_path.string());
    co_return result;
  } catch (const std::exception &e) {
    co_return std::unexpected(format_file_error("Error getting file info", file_path, e));
  }
}

}  // namespace Utils::File