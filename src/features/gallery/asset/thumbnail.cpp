module;

#include <format>
#include <iostream>

module Features.Gallery.Asset.Thumbnail;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.State;
import Features.Gallery.Asset.Repository;
import Core.Database;
import Utils.Image;
import Utils.Path;
import Utils.Logger;

namespace Features::Gallery::Asset::Thumbnail {

// ============= 缩略图路径管理 =============

auto ensure_thumbnails_directory_exists(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  // 如果状态中已经有缩略图目录路径，确保目录存在
  if (!app_state.gallery->thumbnails_directory.empty()) {
    auto ensure_dir_result =
        Utils::Path::EnsureDirectoryExists(app_state.gallery->thumbnails_directory);
    if (!ensure_dir_result) {
      return std::unexpected("Failed to ensure thumbnails directory exists: " +
                             ensure_dir_result.error());
    }
    return {};
  }

  // 否则，计算路径、创建目录并保存到状态中
  auto exe_dir_result = Utils::Path::GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  auto thumbnails_dir = exe_dir_result.value() / "thumbnails";

  // 确保目录存在
  auto ensure_dir_result = Utils::Path::EnsureDirectoryExists(thumbnails_dir);
  if (!ensure_dir_result) {
    return std::unexpected("Failed to create thumbnails directory: " + ensure_dir_result.error());
  }

  app_state.gallery->thumbnails_directory = thumbnails_dir;

  return {};
}

// 确保缩略图路径存在
auto ensure_thumbnail_path(Core::State::AppState& app_state, const std::string& file_hash,
                           uint32_t width, uint32_t height)
    -> std::expected<std::filesystem::path, std::string> {
  // 检查缩略图目录是否已初始化
  if (app_state.gallery->thumbnails_directory.empty()) {
    auto ensure_result = ensure_thumbnails_directory_exists(app_state);
    if (!ensure_result) {
      return std::unexpected(ensure_result.error());
    }
  }

  // 使用xxh3 64位哈希的前4位作为两级子目录
  std::string level1 = file_hash.substr(0, 2);
  std::string level2 = file_hash.substr(2, 2);
  std::string filename = std::format("{}_{:d}x{:d}.webp", file_hash, width, height);

  // 构建完整路径
  auto thumbnail_path = app_state.gallery->thumbnails_directory / level1 / level2 / filename;

  // 确保子目录存在
  std::error_code ec;
  auto parent_dir = thumbnail_path.parent_path();
  if (!std::filesystem::exists(parent_dir, ec)) {
    if (!std::filesystem::create_directories(parent_dir, ec)) {
      return std::unexpected("Failed to create thumbnail subdirectories: " + ec.message());
    }
  }

  return thumbnail_path;
}

// ============= 缩略图清理功能 =============

auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<void, std::string> {
  if (app_state.gallery->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }

  if (!asset.hash.has_value() || asset.hash->empty()) {
    return std::unexpected("Asset has no file hash, cannot determine thumbnail files");
  }

  int deleted_count = 0;
  std::error_code ec;
  const std::string& file_hash = *asset.hash;

  std::string level1 = file_hash.substr(0, 2);
  std::string level2 = file_hash.substr(2, 2);
  auto target_subdir = app_state.gallery->thumbnails_directory / level1 / level2;

  if (std::filesystem::exists(target_subdir, ec) && !ec) {
    // 遍历特定子目录，删除对应的缩略图
    for (const auto& entry : std::filesystem::directory_iterator(target_subdir, ec)) {
      if (ec) break;

      if (entry.is_regular_file(ec) && entry.path().extension() == ".webp") {
        std::string filename = entry.path().filename().string();

        // 检查文件名是否以文件哈希开头
        if (filename.starts_with(file_hash + "_")) {
          std::error_code remove_ec;
          if (std::filesystem::remove(entry.path(), remove_ec)) {
            deleted_count++;
            Logger().debug("Deleted thumbnail: {}", entry.path().string());
          } else {
            Logger().warn("Failed to delete thumbnail {}: {}", entry.path().string(),
                          remove_ec.message());
          }
        }
      }
    }
  }

  if (ec) {
    return std::unexpected("Error during thumbnail deletion: " + ec.message());
  }

  if (deleted_count > 0) {
    Logger().info("Deleted {} thumbnail(s) for asset ID {}", deleted_count, asset.id);
  }

  return {};
}

auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string> {
  // 直接从状态中获取缩略图目录路径
  if (app_state.gallery->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }
  auto thumbnails_dir = app_state.gallery->thumbnails_directory;

  std::error_code ec;
  if (!std::filesystem::exists(thumbnails_dir, ec)) {
    return 0;  // 目录不存在，没有需要清理的
  }

  // 使用 load_asset_cache 获取所有资产的文件哈希集合
  std::unordered_set<std::string> all_file_hashes;
  auto cache_result = Repository::load_asset_cache(app_state);
  if (cache_result) {
    for (const auto& [filepath, metadata] : *cache_result) {
      if (metadata.hash.has_value() && !metadata.hash->empty()) {
        all_file_hashes.insert(*metadata.hash);
      }
    }
  }

  int deleted_count = 0;

  // 使用递归遍历器以支持分层目录结构
  for (const auto& entry : std::filesystem::recursive_directory_iterator(thumbnails_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to iterate thumbnails directory recursively: " + ec.message());
    }

    if (entry.is_regular_file(ec) && entry.path().extension() == ".webp") {
      std::string filename = entry.path().filename().string();

      // 从文件名提取文件哈希（格式：{file_hash}_{width}x{height}.webp）
      auto underscore_pos = filename.find('_');
      if (underscore_pos != std::string::npos) {
        std::string file_hash = filename.substr(0, underscore_pos);

        // 检查文件哈希是否存在于任何资产中
        if (all_file_hashes.find(file_hash) == all_file_hashes.end()) {
          // 文件哈希不存在于任何资产中，删除缩略图
          std::error_code remove_ec;
          if (std::filesystem::remove(entry.path(), remove_ec)) {
            deleted_count++;
            Logger().debug("Deleted orphaned thumbnail: {}", entry.path().string());
          } else {
            Logger().warn("Failed to delete orphaned thumbnail {}: {}", entry.path().string(),
                          remove_ec.message());
          }
        }
      }
    }
  }

  if (ec) {
    return std::unexpected("Error during orphaned thumbnails cleanup: " + ec.message());
  }

  return deleted_count;
}

// ============= 基于哈希的缩略图生成 =============

// 使用文件哈希生成缩略图（多线程优化版）
auto generate_thumbnail(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& source_file, const std::string& file_hash,
                        uint32_t max_width, uint32_t max_height)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    auto thumbnail_path_result = ensure_thumbnail_path(app_state, file_hash, max_width, max_height);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 检查缩略图是否已存在（基于哈希的去重）
    if (std::filesystem::exists(thumbnail_path)) {
      Logger().debug("Thumbnail already exists, reusing: {}", thumbnail_path.string());
      return thumbnail_path;
    }

    // 生成 WebP 缩略图
    Utils::Image::WebPEncodeOptions options;
    options.quality = 75.0f;  // 默认质量

    auto webp_result = Utils::Image::generate_webp_thumbnail(wic_factory, source_file, max_width,
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

    Logger().debug("Generated thumbnail: {} ({} bytes)", thumbnail_path.string(),
                   webp_data.data.size());

    return thumbnail_path;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in generate_thumbnail: " + std::string(e.what()));
  }
}

// ============= 缩略图统计功能 =============

auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<AssetThumbnailStats, std::string> {
  AssetThumbnailStats stats = {};

  // 直接从状态中获取缩略图目录路径
  if (app_state.gallery->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }
  auto thumbnails_dir = app_state.gallery->thumbnails_directory;
  stats.thumbnails_directory = thumbnails_dir.string();

  std::error_code ec;
  if (!std::filesystem::exists(thumbnails_dir, ec)) {
    return stats;  // 返回空统计
  }

  // 使用 load_asset_cache 获取所有资产的文件哈希集合
  std::unordered_set<std::string> all_file_hashes;
  auto cache_result = Repository::load_asset_cache(app_state);
  if (cache_result) {
    for (const auto& [filepath, metadata] : *cache_result) {
      if (metadata.hash.has_value() && !metadata.hash->empty()) {
        all_file_hashes.insert(*metadata.hash);
      }
    }
  }

  int64_t total_size = 0;
  int total_thumbnails = 0;
  int orphaned_thumbnails = 0;

  // 使用递归遍历器以支持分层目录结构
  for (const auto& entry : std::filesystem::recursive_directory_iterator(thumbnails_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to iterate thumbnails directory recursively: " + ec.message());
    }

    if (entry.is_regular_file(ec) && entry.path().extension() == ".webp") {
      total_thumbnails++;

      // 累加文件大小
      std::error_code size_ec;
      auto file_size = std::filesystem::file_size(entry.path(), size_ec);
      if (!size_ec) {
        total_size += file_size;
      }

      // 检查是否为孤立缩略图
      std::string filename = entry.path().filename().string();
      auto underscore_pos = filename.find('_');
      if (underscore_pos != std::string::npos) {
        std::string file_hash = filename.substr(0, underscore_pos);

        // 检查文件哈希是否存在于任何资产中
        if (all_file_hashes.find(file_hash) == all_file_hashes.end()) {
          orphaned_thumbnails++;  // 文件哈希不存在于任何资产中，当作孤立处理
        }
      }
    }
  }

  if (ec) {
    return std::unexpected("Error during thumbnail stats collection: " + ec.message());
  }

  stats.total_thumbnails = total_thumbnails;
  stats.total_size = total_size;
  stats.orphaned_thumbnails = orphaned_thumbnails;

  return stats;
}

}  // namespace Features::Gallery::Asset::Thumbnail
