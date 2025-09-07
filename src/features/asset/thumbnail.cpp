module;

#include <format>

module Features.Asset.Thumbnail;

import std;
import Core.State;
import Features.Asset.State;
import Features.Asset.Types;
import Features.Asset.Repository;
import Utils.Image;
import Utils.Path;
import Utils.Logger;

namespace Features::Asset::Thumbnail {

// ============= 缩略图路径管理 =============

auto ensure_thumbnails_directory_exists(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  // 如果状态中已经有缩略图目录路径，确保目录存在
  if (!app_state.asset->thumbnails_directory.empty()) {
    auto ensure_dir_result =
        Utils::Path::EnsureDirectoryExists(app_state.asset->thumbnails_directory);
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

  app_state.asset->thumbnails_directory = thumbnails_dir;

  return {};
}

auto generate_thumbnail_filename(int64_t asset_id, const std::string& original_filename)
    -> std::string {
  // 使用资产ID和原文件名的组合来生成唯一的缩略图文件名
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

  return std::format("{}_{}.webp", asset_id, stem);
}

auto get_thumbnail_path(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<std::filesystem::path, std::string> {
  // 直接从状态中获取缩略图目录路径
  if (app_state.asset->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }

  auto thumbnail_filename = generate_thumbnail_filename(asset.id, asset.filename);
  return app_state.asset->thumbnails_directory / thumbnail_filename;
}

// ============= 缩略图生成功能 =============

auto generate_thumbnail_for_asset(Core::State::AppState& app_state,
                                  Utils::Image::WICFactory& wic_factory, const Types::Asset& asset,
                                  uint32_t max_width, uint32_t max_height,
                                  const Utils::Image::WebPEncodeOptions& options)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    // 只为图片类型生成缩略图
    if (asset.type != "photo") {
      return std::unexpected("Cannot generate thumbnail for non-photo asset type");
    }

     // 获取缩略图路径
    auto thumbnail_path_result = get_thumbnail_path(app_state, asset);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 检查原文件是否存在
    std::filesystem::path source_path(asset.filepath);
    if (!std::filesystem::exists(source_path)) {
      return std::unexpected("Source file does not exist: " + asset.filepath);
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

    Logger().debug("Generated thumbnail for {}: {} ({} bytes)", asset.filename,
                   thumbnail_path.string(), webp_data.data.size());

    return thumbnail_path;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in generate_thumbnail_for_media: " + std::string(e.what()));
  }
}

auto batch_generate_thumbnails(Core::State::AppState& app_state,
                               Utils::Image::WICFactory& wic_factory,
                               const std::vector<Types::Asset>& assets, uint32_t max_width,
                               uint32_t max_height)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
  try {
    std::vector<std::filesystem::path> generated_thumbnails;
    generated_thumbnails.reserve(assets.size());

    Utils::Image::WebPEncodeOptions options;
    options.quality = 75.0f;  // 默认质量

    for (const auto& asset : assets) {
      if (asset.type == "photo") {
        auto thumbnail_result = generate_thumbnail_for_asset(app_state, wic_factory, asset,
                                                             max_width, max_height, options);

        if (thumbnail_result) {
          generated_thumbnails.push_back(thumbnail_result.value());
        } else {
          Logger().warn("Failed to generate thumbnail for {}: {}", asset.filename,
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

// ============= 缩略图清理功能 =============

auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<void, std::string> {
  try {
    auto thumbnail_path_result = get_thumbnail_path(app_state, asset);
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

auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string> {
  try {
    // 直接从状态中获取缩略图目录路径
    if (app_state.asset->thumbnails_directory.empty()) {
      return std::unexpected("Thumbnails directory not initialized");
    }
    auto thumbnails_dir = app_state.asset->thumbnails_directory;

    if (!std::filesystem::exists(thumbnails_dir)) {
      return 0;  // 目录不存在，没有需要清理的
    }

    int deleted_count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(thumbnails_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".webp") {
        std::string filename = entry.path().filename().string();

        // 从文件名提取资产ID（格式：{asset_id}_{filename}.webp）
        auto underscore_pos = filename.find('_');
        if (underscore_pos != std::string::npos) {
          try {
            std::string id_str = filename.substr(0, underscore_pos);
            int64_t asset_id = std::stoll(id_str);

            // 检查资产项是否还存在
            auto media_exists_result = Repository::get_asset_by_id(app_state, asset_id);
            if (!media_exists_result || !media_exists_result->has_value()) {
              // 资产项不存在，删除缩略图
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

// ============= 缩略图统计功能 =============

auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<AssetThumbnailStats, std::string> {
  try {
    AssetThumbnailStats stats = {};

    // 直接从状态中获取缩略图目录路径
    if (app_state.asset->thumbnails_directory.empty()) {
      return std::unexpected("Thumbnails directory not initialized");
    }
    auto thumbnails_dir = app_state.asset->thumbnails_directory;
    stats.thumbnails_directory = thumbnails_dir.string();

    if (!std::filesystem::exists(thumbnails_dir)) {
      return stats;  // 返回空统计
    }

    int64_t total_size = 0;
    int total_thumbnails = 0;
    int orphaned_thumbnails = 0;

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
            int64_t asset_id = std::stoll(id_str);

            auto media_exists_result = Repository::get_asset_by_id(app_state, asset_id);
            if (!media_exists_result || !media_exists_result->has_value()) {
              orphaned_thumbnails++;
            }
          } catch (const std::exception&) {
            orphaned_thumbnails++;  // 无法解析ID，当作孤立处理
          }
        }
      }
    }

    stats.total_thumbnails = total_thumbnails;
    stats.total_size = total_size;
    stats.orphaned_thumbnails = orphaned_thumbnails;

    return stats;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_stats: " + std::string(e.what()));
  }
}

}  // namespace Features::Asset::Thumbnail
