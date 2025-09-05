module;

#include <format>

module Features.Media.Thumbnail;

import std;
import Core.State;
import Features.Media.Types;
import Features.Media.Database;
import Utils.Image;
import Utils.Path;
import Utils.Logger;

namespace Features::Media::Thumbnail {

// ============= 缩略图路径管理 =============

auto get_thumbnails_directory() -> std::expected<std::filesystem::path, std::string> {
  try {
    auto exe_dir_result = Utils::Path::GetExecutableDirectory();
    if (!exe_dir_result) {
      return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
    }

    return exe_dir_result.value() / "thumbnails";

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnails_directory: " + std::string(e.what()));
  }
}

auto ensure_thumbnails_directory_exists() -> std::expected<void, std::string> {
  try {
    auto thumbnails_dir_result = get_thumbnails_directory();
    if (!thumbnails_dir_result) {
      return std::unexpected(thumbnails_dir_result.error());
    }

    auto thumbnails_dir = thumbnails_dir_result.value();

    if (!std::filesystem::exists(thumbnails_dir)) {
      std::error_code ec;
      std::filesystem::create_directories(thumbnails_dir, ec);
      if (ec) {
        return std::unexpected("Failed to create thumbnails directory: " + ec.message());
      }
      Logger().info("Created thumbnails directory: {}", thumbnails_dir.string());
    }

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception in ensure_thumbnails_directory_exists: " +
                           std::string(e.what()));
  }
}

auto generate_thumbnail_filename(int64_t media_id, const std::string& original_filename)
    -> std::string {
  // 使用媒体ID和原文件名的组合来生成唯一的缩略图文件名
  std::filesystem::path original_path(original_filename);
  std::string stem = original_path.stem().string();

  // 替换可能有问题的字符
  std::ranges::replace_if(
      stem,
      [](char c) {
        return c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
               c == '>' || c == '|';
      },
      '_');

  return std::format("{}_{}.webp", media_id, stem);
}

auto get_thumbnail_path(const Types::MediaItem& media_item)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    auto thumbnails_dir_result = get_thumbnails_directory();
    if (!thumbnails_dir_result) {
      return std::unexpected(thumbnails_dir_result.error());
    }

    auto thumbnail_filename = generate_thumbnail_filename(media_item.id, media_item.filename);
    return thumbnails_dir_result.value() / thumbnail_filename;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_path: " + std::string(e.what()));
  }
}

// ============= 缩略图生成功能 =============

auto generate_thumbnail_for_media(Core::State::AppState& app_state,
                                  Utils::Image::WICFactory& wic_factory,
                                  const Types::MediaItem& media_item, uint32_t max_width,
                                  uint32_t max_height,
                                  const Utils::Image::WebPEncodeOptions& options)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    // 只为图片类型生成缩略图
    if (media_item.type != "photo") {
      return std::unexpected("Cannot generate thumbnail for non-photo media type");
    }

    // 确保缩略图目录存在
    auto ensure_dir_result = ensure_thumbnails_directory_exists();
    if (!ensure_dir_result) {
      return std::unexpected(ensure_dir_result.error());
    }

    // 获取缩略图路径
    auto thumbnail_path_result = get_thumbnail_path(media_item);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 检查原文件是否存在
    std::filesystem::path source_path(media_item.filepath);
    if (!std::filesystem::exists(source_path)) {
      return std::unexpected("Source file does not exist: " + media_item.filepath);
    }

    // 生成 WebP 缩略图
    auto webp_result = Utils::Image::generate_webp_thumbnail(wic_factory, source_path, max_width,
                                                             max_height, options);
    if (!webp_result) {
      return std::unexpected("Failed to generate WebP thumbnail: " + webp_result.error());
    }

    auto webp_data = webp_result.value();

    // 保存缩略图到文件
    std::ofstream thumbnail_file(thumbnail_path, std::ios::binary);
    if (!thumbnail_file) {
      return std::unexpected("Failed to create thumbnail file: " + thumbnail_path.string());
    }

    thumbnail_file.write(reinterpret_cast<const char*>(webp_data.data.data()),
                         webp_data.data.size());
    thumbnail_file.close();

    if (!thumbnail_file.good()) {
      return std::unexpected("Failed to write thumbnail data to file");
    }

    Logger().debug("Generated thumbnail for {}: {} ({} bytes)", media_item.filename,
                   thumbnail_path.string(), webp_data.data.size());

    return thumbnail_path;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in generate_thumbnail_for_media: " + std::string(e.what()));
  }
}

auto batch_generate_thumbnails(Core::State::AppState& app_state,
                               Utils::Image::WICFactory& wic_factory,
                               const std::vector<Types::MediaItem>& media_items, uint32_t max_width,
                               uint32_t max_height)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
  try {
    std::vector<std::filesystem::path> generated_thumbnails;
    generated_thumbnails.reserve(media_items.size());

    Utils::Image::WebPEncodeOptions options;
    options.quality = 75.0f;  // 默认质量

    for (const auto& media_item : media_items) {
      if (media_item.type == "photo") {
        auto thumbnail_result = generate_thumbnail_for_media(app_state, wic_factory, media_item,
                                                             max_width, max_height, options);

        if (thumbnail_result) {
          generated_thumbnails.push_back(thumbnail_result.value());
        } else {
          Logger().warn("Failed to generate thumbnail for {}: {}", media_item.filename,
                        thumbnail_result.error());
          // 继续处理其他项目，不因单个失败而中断
        }
      }
    }

    return generated_thumbnails;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in batch_generate_thumbnails: " + std::string(e.what()));
  }
}

// ============= 缩略图访问功能 =============

auto thumbnail_exists(const std::filesystem::path& thumbnail_path) -> bool {
  try {
    return std::filesystem::exists(thumbnail_path) &&
           std::filesystem::is_regular_file(thumbnail_path);
  } catch (const std::filesystem::filesystem_error&) {
    return false;
  }
}

auto get_thumbnail_data(Core::State::AppState& app_state, int64_t media_id)
    -> std::expected<std::vector<uint8_t>, std::string> {
  try {
    // 从数据库获取媒体项信息
    auto media_item_result = Database::get_media_item_by_id(app_state, media_id);
    if (!media_item_result) {
      return std::unexpected("Failed to get media item: " + media_item_result.error());
    }

    if (!media_item_result->has_value()) {
      return std::unexpected("Media item not found");
    }

    auto media_item = media_item_result->value();

    // 获取缩略图路径
    auto thumbnail_path_result = get_thumbnail_path(media_item);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 检查缩略图是否存在
    if (!thumbnail_exists(thumbnail_path)) {
      return std::unexpected("Thumbnail does not exist: " + thumbnail_path.string());
    }

    // 读取缩略图数据
    std::ifstream thumbnail_file(thumbnail_path, std::ios::binary);
    if (!thumbnail_file) {
      return std::unexpected("Failed to open thumbnail file: " + thumbnail_path.string());
    }

    // 获取文件大小
    thumbnail_file.seekg(0, std::ios::end);
    auto file_size = thumbnail_file.tellg();
    thumbnail_file.seekg(0, std::ios::beg);

    // 读取数据
    std::vector<uint8_t> thumbnail_data(file_size);
    thumbnail_file.read(reinterpret_cast<char*>(thumbnail_data.data()), file_size);

    if (!thumbnail_file.good()) {
      return std::unexpected("Failed to read thumbnail data");
    }

    return thumbnail_data;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_data: " + std::string(e.what()));
  }
}

auto get_thumbnail_info(const std::filesystem::path& thumbnail_path)
    -> std::expected<std::tuple<uint32_t, uint32_t, int64_t>, std::string> {
  try {
    if (!thumbnail_exists(thumbnail_path)) {
      return std::unexpected("Thumbnail file does not exist");
    }

    // 获取文件大小
    std::error_code ec;
    auto file_size = std::filesystem::file_size(thumbnail_path, ec);
    if (ec) {
      return std::unexpected("Failed to get thumbnail file size: " + ec.message());
    }

    // 对于 WebP 文件，我们可以尝试解析尺寸信息
    // 这里简化处理，返回默认值，实际可以解析 WebP 头部获取真实尺寸
    uint32_t width = 0;
    uint32_t height = 0;

    // TODO: 解析 WebP 文件头获取真实尺寸
    // 暂时返回估算值

    return std::make_tuple(width, height, static_cast<int64_t>(file_size));

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_info: " + std::string(e.what()));
  }
}

// ============= 缩略图清理功能 =============

auto delete_thumbnail(const Types::MediaItem& media_item) -> std::expected<void, std::string> {
  try {
    auto thumbnail_path_result = get_thumbnail_path(media_item);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    if (thumbnail_exists(thumbnail_path)) {
      std::error_code ec;
      std::filesystem::remove(thumbnail_path, ec);
      if (ec) {
        return std::unexpected("Failed to delete thumbnail: " + ec.message());
      }
      Logger().debug("Deleted thumbnail: {}", thumbnail_path.string());
    }

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception in delete_thumbnail: " + std::string(e.what()));
  }
}

auto batch_delete_thumbnails(const std::vector<Types::MediaItem>& media_items)
    -> std::expected<int, std::string> {
  try {
    int deleted_count = 0;

    for (const auto& media_item : media_items) {
      auto delete_result = delete_thumbnail(media_item);
      if (delete_result) {
        deleted_count++;
      } else {
        Logger().warn("Failed to delete thumbnail for {}: {}", media_item.filename,
                      delete_result.error());
      }
    }

    return deleted_count;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in batch_delete_thumbnails: " + std::string(e.what()));
  }
}

auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string> {
  try {
    auto thumbnails_dir_result = get_thumbnails_directory();
    if (!thumbnails_dir_result) {
      return std::unexpected(thumbnails_dir_result.error());
    }
    auto thumbnails_dir = thumbnails_dir_result.value();

    if (!std::filesystem::exists(thumbnails_dir)) {
      return 0;  // 目录不存在，没有需要清理的
    }

    int deleted_count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(thumbnails_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".webp") {
        std::string filename = entry.path().filename().string();

        // 从文件名提取媒体ID（格式：{media_id}_{filename}.webp）
        auto underscore_pos = filename.find('_');
        if (underscore_pos != std::string::npos) {
          try {
            std::string id_str = filename.substr(0, underscore_pos);
            int64_t media_id = std::stoll(id_str);

            // 检查媒体项是否还存在
            auto media_exists_result = Database::get_media_item_by_id(app_state, media_id);
            if (!media_exists_result || !media_exists_result->has_value()) {
              // 媒体项不存在，删除缩略图
              std::error_code ec;
              std::filesystem::remove(entry.path(), ec);
              if (!ec) {
                deleted_count++;
                Logger().debug("Deleted orphaned thumbnail: {}", entry.path().string());
              }
            }
          } catch (const std::exception&) {
            // 无法解析文件名，跳过
            continue;
          }
        }
      }
    }

    return deleted_count;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in cleanup_orphaned_thumbnails: " + std::string(e.what()));
  }
}

// ============= 缩略图验证功能 =============

auto needs_thumbnail_regeneration(const Types::MediaItem& media_item)
    -> std::expected<bool, std::string> {
  try {
    auto thumbnail_path_result = get_thumbnail_path(media_item);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 如果缩略图不存在，需要生成
    if (!thumbnail_exists(thumbnail_path)) {
      return true;
    }

    // 检查原文件是否比缩略图更新
    std::filesystem::path source_path(media_item.filepath);
    if (!std::filesystem::exists(source_path)) {
      return false;  // 原文件不存在，不需要重新生成
    }

    std::error_code ec;
    auto source_time = std::filesystem::last_write_time(source_path, ec);
    if (ec) {
      return true;  // 无法获取原文件时间，假设需要重新生成
    }

    auto thumbnail_time = std::filesystem::last_write_time(thumbnail_path, ec);
    if (ec) {
      return true;  // 无法获取缩略图时间，重新生成
    }

    return source_time > thumbnail_time;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in needs_thumbnail_regeneration: " + std::string(e.what()));
  }
}

auto validate_thumbnail_integrity(const std::filesystem::path& thumbnail_path)
    -> std::expected<bool, std::string> {
  try {
    if (!thumbnail_exists(thumbnail_path)) {
      return false;
    }

    // 简单验证：检查文件是否可读且非空
    std::ifstream file(thumbnail_path, std::ios::binary);
    if (!file) {
      return false;
    }

    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();

    // WebP 文件应该至少有一些字节
    return file_size > 12;  // WebP 头部至少 12 字节

  } catch (const std::exception& e) {
    return std::unexpected("Exception in validate_thumbnail_integrity: " + std::string(e.what()));
  }
}

// ============= 缩略图统计功能 =============

auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<ThumbnailStats, std::string> {
  try {
    ThumbnailStats stats = {};

    auto thumbnails_dir_result = get_thumbnails_directory();
    if (!thumbnails_dir_result) {
      return std::unexpected(thumbnails_dir_result.error());
    }
    auto thumbnails_dir = thumbnails_dir_result.value();
    stats.thumbnails_directory = thumbnails_dir.string();

    if (!std::filesystem::exists(thumbnails_dir)) {
      return stats;  // 返回空统计
    }

    int64_t total_size = 0;
    int total_thumbnails = 0;
    int orphaned_thumbnails = 0;
    int corrupted_thumbnails = 0;

    for (const auto& entry : std::filesystem::directory_iterator(thumbnails_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".webp") {
        total_thumbnails++;

        // 累加文件大小
        std::error_code ec;
        auto file_size = std::filesystem::file_size(entry.path(), ec);
        if (!ec) {
          total_size += file_size;
        }

        // 检查是否为孤立缩略图
        std::string filename = entry.path().filename().string();
        auto underscore_pos = filename.find('_');
        if (underscore_pos != std::string::npos) {
          try {
            std::string id_str = filename.substr(0, underscore_pos);
            int64_t media_id = std::stoll(id_str);

            auto media_exists_result = Database::get_media_item_by_id(app_state, media_id);
            if (!media_exists_result || !media_exists_result->has_value()) {
              orphaned_thumbnails++;
            }
          } catch (const std::exception&) {
            orphaned_thumbnails++;  // 无法解析ID，当作孤立处理
          }
        }

        // 检查文件完整性
        auto integrity_result = validate_thumbnail_integrity(entry.path());
        if (integrity_result && !integrity_result.value()) {
          corrupted_thumbnails++;
        }
      }
    }

    stats.total_thumbnails = total_thumbnails;
    stats.total_size = total_size;
    stats.orphaned_thumbnails = orphaned_thumbnails;
    stats.corrupted_thumbnails = corrupted_thumbnails;

    return stats;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_stats: " + std::string(e.what()));
  }
}

// ============= 高级功能 =============

auto execute_thumbnail_task(Utils::Image::WICFactory& wic_factory,
                            const ThumbnailGenerationTask& task) -> ThumbnailGenerationResult {
  ThumbnailGenerationResult result = {};
  result.thumbnail_path = task.thumbnail_path;

  try {
    // 检查源文件是否存在
    if (!std::filesystem::exists(task.source_path)) {
      result.error_message = "Source file does not exist";
      return result;
    }

    // 生成 WebP 缩略图
    auto webp_result = Utils::Image::generate_webp_thumbnail(
        wic_factory, task.source_path, task.max_width, task.max_height, task.options);
    if (!webp_result) {
      result.error_message = "Failed to generate WebP thumbnail: " + webp_result.error();
      return result;
    }

    auto webp_data = webp_result.value();
    result.generated_width = webp_data.width;
    result.generated_height = webp_data.height;

    // 确保目标目录存在
    if (task.thumbnail_path.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(task.thumbnail_path.parent_path(), ec);
      if (ec && !std::filesystem::exists(task.thumbnail_path.parent_path())) {
        result.error_message = "Failed to create thumbnail directory: " + ec.message();
        return result;
      }
    }

    // 保存缩略图到文件
    std::ofstream thumbnail_file(task.thumbnail_path, std::ios::binary);
    if (!thumbnail_file) {
      result.error_message = "Failed to create thumbnail file";
      return result;
    }

    thumbnail_file.write(reinterpret_cast<const char*>(webp_data.data.data()),
                         webp_data.data.size());
    thumbnail_file.close();

    if (!thumbnail_file.good()) {
      result.error_message = "Failed to write thumbnail data";
      return result;
    }

    result.file_size = static_cast<int64_t>(webp_data.data.size());
    result.success = true;

    return result;

  } catch (const std::exception& e) {
    result.error_message = "Exception in execute_thumbnail_task: " + std::string(e.what());
    return result;
  }
}

}  // namespace Features::Media::Thumbnail
