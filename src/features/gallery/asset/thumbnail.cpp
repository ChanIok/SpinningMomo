module;

module Features.Gallery.Asset.Thumbnail;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.State;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Service;
import Core.Database;
import Utils.Image;
import Utils.Media.VideoAsset;
import Utils.Path;
import Utils.Logger;

namespace Features::Gallery::Asset::Thumbnail {

// “一个缩略图 hash 应该如何被满足”的最小工作单元。
// 一个 hash 可能对应多个源文件路径；修复时只需找到其中任意一个仍存在的源文件。
struct ExpectedThumbnailEntry {
  std::string hash;
  std::string type;
  std::vector<std::filesystem::path> source_paths;
};

// 内部汇总结构：只关注“缺失缩略图补回”这一件事。
struct MissingThumbnailRepairSummary {
  int candidate_hashes = 0;
  int missing_thumbnails = 0;
  int repaired_thumbnails = 0;
  int failed_repairs = 0;
  int skipped_missing_sources = 0;
};

auto extract_hash_from_thumbnail(const std::filesystem::path& thumbnail_path)
    -> std::optional<std::string>;

auto query_thumbnail_candidates(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Asset>, std::string> {
  std::string sql = R"(
    SELECT id, name, path, type,
           NULL AS dominant_color_hex,
           rating, review_flag,
           description, width, height, size, extension, mime_type, hash,
           NULL AS root_id, NULL AS relative_path, folder_id,
           file_created_at, file_modified_at,
           created_at, updated_at
    FROM assets
    WHERE type IN ('photo', 'video')
      AND hash IS NOT NULL
      AND hash != ''
      AND path IS NOT NULL
      AND path != ''
  )";

  auto result = Core::Database::query<Types::Asset>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to query thumbnail candidates: " + result.error());
  }

  return result.value();
}

// 局部修复时允许只处理某个 root；全局对账则传 nullopt 表示不过滤。
auto normalize_thumbnail_root_filter(std::optional<std::filesystem::path> root_directory)
    -> std::expected<std::optional<std::filesystem::path>, std::string> {
  if (!root_directory.has_value()) {
    return std::optional<std::filesystem::path>{std::nullopt};
  }

  auto normalized_root_result = Utils::Path::NormalizePath(root_directory.value());
  if (!normalized_root_result) {
    return std::unexpected("Failed to normalize thumbnail repair root: " +
                           normalized_root_result.error());
  }

  return std::optional<std::filesystem::path>{normalized_root_result.value()};
}

// 从 DB 收集“理论上应当存在缩略图”的集合，并按 hash 去重。
auto collect_expected_thumbnail_entries(Core::State::AppState& app_state,
                                        std::optional<std::filesystem::path> root_directory)
    -> std::expected<std::unordered_map<std::string, ExpectedThumbnailEntry>, std::string> {
  auto normalized_root_result = normalize_thumbnail_root_filter(root_directory);
  if (!normalized_root_result) {
    return std::unexpected(normalized_root_result.error());
  }
  const auto& normalized_root_directory = normalized_root_result.value();

  auto candidates_result = query_thumbnail_candidates(app_state);
  if (!candidates_result) {
    return std::unexpected(candidates_result.error());
  }

  std::unordered_map<std::string, ExpectedThumbnailEntry> entries;
  for (const auto& asset : candidates_result.value()) {
    if (!asset.hash.has_value() || asset.hash->empty()) {
      continue;
    }

    std::filesystem::path asset_path(asset.path);
    if (normalized_root_directory.has_value() &&
        !Utils::Path::IsPathWithinBase(asset_path, normalized_root_directory.value())) {
      continue;
    }

    auto& entry = entries[asset.hash.value()];
    if (entry.hash.empty()) {
      entry.hash = asset.hash.value();
      entry.type = asset.type;
    }
    entry.source_paths.push_back(std::move(asset_path));
  }

  return entries;
}

// 从缩略图目录扫描“当前实际存在的缩略图集合”。
auto scan_existing_thumbnail_files(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, std::filesystem::path>, std::string> {
  if (app_state.gallery->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }

  auto thumbnails_dir = app_state.gallery->thumbnails_directory;
  std::error_code ec;
  if (!std::filesystem::exists(thumbnails_dir, ec)) {
    return std::unordered_map<std::string, std::filesystem::path>{};
  }
  if (ec) {
    return std::unexpected("Failed to check thumbnails directory existence: " + ec.message());
  }

  std::unordered_map<std::string, std::filesystem::path> entries;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(thumbnails_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to iterate thumbnails directory recursively: " + ec.message());
    }

    if (!entry.is_regular_file(ec) || entry.path().extension() != ".webp") {
      continue;
    }
    if (ec) {
      return std::unexpected("Failed to inspect thumbnail entry: " + ec.message());
    }

    auto hash_result = extract_hash_from_thumbnail(entry.path());
    if (!hash_result) {
      continue;
    }

    entries.try_emplace(*hash_result, entry.path());
  }

  if (ec) {
    return std::unexpected("Error during thumbnail inventory scan: " + ec.message());
  }

  return entries;
}

// 只负责补“缺失缩略图”；孤儿删除由上层全局对账处理。
// 如果调用方已经事先拿到了 existing_hashes，就不必再逐个 hash 查磁盘了。
auto repair_expected_thumbnail_entries(
    Core::State::AppState& app_state,
    const std::unordered_map<std::string, ExpectedThumbnailEntry>& expected_entries,
    const std::unordered_set<std::string>* existing_hashes, std::uint32_t short_edge_size)
    -> MissingThumbnailRepairSummary {
  MissingThumbnailRepairSummary stats;
  stats.candidate_hashes = static_cast<int>(expected_entries.size());

  std::optional<Utils::Image::WICFactory> wic_factory;

  for (const auto& [hash, entry] : expected_entries) {
    bool thumbnail_exists = false;

    if (existing_hashes != nullptr) {
      thumbnail_exists = existing_hashes->contains(hash);
    } else {
      auto thumbnail_path_result = ensure_thumbnail_path(app_state, hash);
      if (!thumbnail_path_result) {
        stats.failed_repairs++;
        Logger().warn("Failed to resolve thumbnail path for hash {}: {}", hash,
                      thumbnail_path_result.error());
        continue;
      }

      std::error_code exists_ec;
      thumbnail_exists = std::filesystem::exists(thumbnail_path_result.value(), exists_ec);
      if (exists_ec) {
        stats.failed_repairs++;
        Logger().warn("Failed to check thumbnail existence for hash {}: {}", hash,
                      exists_ec.message());
        continue;
      }
    }

    if (thumbnail_exists) {
      continue;
    }

    stats.missing_thumbnails++;

    std::optional<std::filesystem::path> source_path;
    // 同一个 hash 可能来自多份重复内容；这里选任意一份还存在的源文件即可重建缩略图。
    for (const auto& candidate_path : entry.source_paths) {
      std::error_code source_ec;
      bool exists = std::filesystem::exists(candidate_path, source_ec);
      bool is_regular = exists && std::filesystem::is_regular_file(candidate_path, source_ec);
      if (!source_ec && exists && is_regular) {
        source_path = candidate_path;
        break;
      }
    }

    if (!source_path.has_value()) {
      stats.skipped_missing_sources++;
      Logger().debug("Skip thumbnail repair for hash {}: no source file is available", hash);
      continue;
    }

    if (entry.type == "photo") {
      if (!wic_factory.has_value()) {
        auto wic_result = Utils::Image::get_thread_wic_factory();
        if (!wic_result) {
          stats.failed_repairs++;
          Logger().warn("Failed to initialize WIC factory for thumbnail repair: {}",
                        wic_result.error());
          continue;
        }
        wic_factory = std::move(wic_result.value());
      }

      auto repair_result =
          generate_thumbnail(app_state, *wic_factory, *source_path, hash, short_edge_size, false);
      if (!repair_result) {
        stats.failed_repairs++;
        Logger().warn("Failed to repair thumbnail for '{}': {}", source_path->string(),
                      repair_result.error());
        continue;
      }

      stats.repaired_thumbnails++;
      continue;
    }

    if (entry.type == "video") {
      auto video_result =
          Utils::Media::VideoAsset::analyze_video_file(*source_path, short_edge_size);
      if (!video_result) {
        stats.failed_repairs++;
        Logger().warn("Failed to analyze video during thumbnail repair '{}': {}",
                      source_path->string(), video_result.error());
        continue;
      }

      if (!video_result->thumbnail.has_value()) {
        stats.failed_repairs++;
        Logger().warn("Video thumbnail repair yielded no thumbnail data: {}",
                      source_path->string());
        continue;
      }

      auto repair_result = save_thumbnail_data(app_state, hash, *video_result->thumbnail, false);
      if (!repair_result) {
        stats.failed_repairs++;
        Logger().warn("Failed to save repaired video thumbnail for '{}': {}", source_path->string(),
                      repair_result.error());
        continue;
      }

      stats.repaired_thumbnails++;
    }
  }

  return stats;
}

// ============= 缩略图路径管理 =============

auto build_thumbnail_path(const std::filesystem::path& thumbnails_dir, const std::string& file_hash)
    -> std::filesystem::path {
  auto level1 = file_hash.substr(0, 2);
  auto level2 = file_hash.substr(2, 2);
  return thumbnails_dir / level1 / level2 / std::format("{}.webp", file_hash);
}

auto extract_hash_from_thumbnail(const std::filesystem::path& thumbnail_path)
    -> std::optional<std::string> {
  auto stem = thumbnail_path.stem().string();
  if (stem.empty()) {
    return std::nullopt;
  }
  return stem;
}

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
  auto thumbnails_dir_result = Utils::Path::GetAppDataSubdirectory("thumbnails");
  if (!thumbnails_dir_result) {
    return std::unexpected("Failed to get thumbnails directory: " + thumbnails_dir_result.error());
  }

  auto thumbnails_dir = thumbnails_dir_result.value();

  app_state.gallery->thumbnails_directory = thumbnails_dir;

  return {};
}

// 确保缩略图路径存在
auto ensure_thumbnail_path(Core::State::AppState& app_state, const std::string& file_hash)
    -> std::expected<std::filesystem::path, std::string> {
  // 检查缩略图目录是否已初始化
  if (app_state.gallery->thumbnails_directory.empty()) {
    auto ensure_result = ensure_thumbnails_directory_exists(app_state);
    if (!ensure_result) {
      return std::unexpected(ensure_result.error());
    }
  }

  auto thumbnail_path = build_thumbnail_path(app_state.gallery->thumbnails_directory, file_hash);

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

auto repair_missing_thumbnails(Core::State::AppState& app_state,
                               std::optional<std::filesystem::path> root_directory,
                               std::uint32_t short_edge_size)
    -> std::expected<ThumbnailRepairStats, std::string> {
  auto ensure_result = ensure_thumbnails_directory_exists(app_state);
  if (!ensure_result) {
    return std::unexpected(ensure_result.error());
  }

  // 局部修复：只处理“当前 root 缺哪些缩略图”，不删除孤儿缩略图。
  auto expected_entries_result = collect_expected_thumbnail_entries(app_state, root_directory);
  if (!expected_entries_result) {
    return std::unexpected(expected_entries_result.error());
  }

  auto summary = repair_expected_thumbnail_entries(app_state, expected_entries_result.value(),
                                                   nullptr, short_edge_size);
  return ThumbnailRepairStats{.candidate_hashes = summary.candidate_hashes,
                              .missing_thumbnails = summary.missing_thumbnails,
                              .repaired_thumbnails = summary.repaired_thumbnails,
                              .failed_repairs = summary.failed_repairs,
                              .skipped_missing_sources = summary.skipped_missing_sources};
}

auto reconcile_thumbnail_cache(Core::State::AppState& app_state, std::uint32_t short_edge_size)
    -> std::expected<ThumbnailCacheReconcileStats, std::string> {
  auto ensure_result = ensure_thumbnails_directory_exists(app_state);
  if (!ensure_result) {
    return std::unexpected(ensure_result.error());
  }

  // 启动时的全局缓存对账：先拿到 DB 认为“应存在”的集合。
  auto expected_entries_result = collect_expected_thumbnail_entries(app_state, std::nullopt);
  if (!expected_entries_result) {
    return std::unexpected(expected_entries_result.error());
  }

  // 再扫描磁盘上“实际存在”的集合。
  auto existing_entries_result = scan_existing_thumbnail_files(app_state);
  if (!existing_entries_result) {
    return std::unexpected(existing_entries_result.error());
  }

  const auto& expected_entries = expected_entries_result.value();
  const auto& existing_entries = existing_entries_result.value();

  std::unordered_set<std::string> existing_hashes;
  existing_hashes.reserve(existing_entries.size());
  for (const auto& [hash, _] : existing_entries) {
    existing_hashes.insert(hash);
  }

  ThumbnailCacheReconcileStats stats;
  stats.expected_hashes = static_cast<int>(expected_entries.size());
  stats.existing_thumbnails = static_cast<int>(existing_entries.size());

  // 先补 missing，再删 orphan。
  // 对用户体验来说，先恢复可见内容比先回收磁盘空间更重要。
  auto repair_summary = repair_expected_thumbnail_entries(app_state, expected_entries,
                                                          &existing_hashes, short_edge_size);
  stats.missing_thumbnails = repair_summary.missing_thumbnails;
  stats.repaired_thumbnails = repair_summary.repaired_thumbnails;
  stats.failed_repairs = repair_summary.failed_repairs;
  stats.skipped_missing_sources = repair_summary.skipped_missing_sources;

  for (const auto& [hash, thumbnail_path] : existing_entries) {
    if (expected_entries.contains(hash)) {
      continue;
    }

    // 走到这里说明：磁盘上有这个缩略图，但 DB 已不再认为它应该存在。
    stats.orphaned_thumbnails++;
    std::error_code remove_ec;
    if (std::filesystem::remove(thumbnail_path, remove_ec)) {
      stats.deleted_orphaned_thumbnails++;
      Logger().debug("Deleted orphaned thumbnail during cache reconcile: {}",
                     thumbnail_path.string());
    } else {
      stats.failed_orphan_deletions++;
      Logger().warn("Failed to delete orphaned thumbnail during cache reconcile {}: {}",
                    thumbnail_path.string(), remove_ec.message());
    }
  }

  return stats;
}

// ============= 缩略图清理功能 =============

auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<void, std::string> {
  if (app_state.gallery->thumbnails_directory.empty()) {
    return std::unexpected("Thumbnails directory not initialized");
  }

  if (!asset.hash || asset.hash->empty()) {
    return std::unexpected("Asset has no file hash, cannot determine thumbnail files");
  }

  const std::string& file_hash = asset.hash.value();
  auto thumbnail_path = build_thumbnail_path(app_state.gallery->thumbnails_directory, file_hash);
  std::error_code ec;

  bool exists = std::filesystem::exists(thumbnail_path, ec);
  if (ec) {
    return std::unexpected("Failed to check thumbnail existence: " + ec.message());
  }
  if (!exists) {
    return {};
  }

  if (!std::filesystem::remove(thumbnail_path, ec)) {
    return std::unexpected("Failed to delete thumbnail: " + ec.message());
  }

  Logger().debug("Deleted thumbnail: {}", thumbnail_path.string());
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
  auto cache_result = Service::load_asset_cache(app_state);
  if (cache_result) {
    for (const auto& [path, metadata] : cache_result.value()) {
      if (!metadata.hash.empty()) {
        all_file_hashes.insert(metadata.hash);
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
      auto hash_result = extract_hash_from_thumbnail(entry.path());
      if (!hash_result) {
        continue;
      }

      // 文件哈希不存在于任何资产中，删除缩略图
      if (all_file_hashes.find(*hash_result) == all_file_hashes.end()) {
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

  if (ec) {
    return std::unexpected("Error during orphaned thumbnails cleanup: " + ec.message());
  }

  return deleted_count;
}

// ============= 基于哈希的缩略图生成 =============

// 使用文件哈希生成缩略图（按短边等比例缩放）
auto generate_thumbnail(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& source_file, const std::string& file_hash,
                        std::uint32_t short_edge_size, bool force_overwrite)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    auto thumbnail_path_result = ensure_thumbnail_path(app_state, file_hash);
    if (!thumbnail_path_result) {
      return std::unexpected(thumbnail_path_result.error());
    }
    auto thumbnail_path = thumbnail_path_result.value();

    // 检查缩略图是否已存在（基于哈希的去重）
    if (!force_overwrite && std::filesystem::exists(thumbnail_path)) {
      Logger().debug("Thumbnail already exists, reusing: {}", thumbnail_path.string());
      return thumbnail_path;
    }

    // 生成 WebP 缩略图
    Utils::Image::WebPEncodeOptions options;
    options.quality = 90.0f;  // 默认质量

    auto webp_result =
        Utils::Image::generate_webp_thumbnail(wic_factory, source_file, short_edge_size, options);
    if (!webp_result) {
      return std::unexpected("Failed to generate WebP thumbnail: " + webp_result.error());
    }

    return save_thumbnail_data(app_state, file_hash, webp_result.value(), force_overwrite);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in generate_thumbnail: " + std::string(e.what()));
  }
}

// 将已编码 WebP 写入按 file_hash
// 命名的路径；图片解码与视频抽帧共用。存在则跳过，减少扫描并发重复写。
auto save_thumbnail_data(Core::State::AppState& app_state, const std::string& file_hash,
                         const Utils::Image::WebPEncodedResult& webp_data, bool force_overwrite)
    -> std::expected<std::filesystem::path, std::string> {
  auto thumbnail_path_result = ensure_thumbnail_path(app_state, file_hash);
  if (!thumbnail_path_result) {
    return std::unexpected(thumbnail_path_result.error());
  }
  auto thumbnail_path = thumbnail_path_result.value();

  if (!force_overwrite && std::filesystem::exists(thumbnail_path)) {
    Logger().debug("Thumbnail already exists, reusing: {}", thumbnail_path.string());
    return thumbnail_path;
  }

  std::ofstream thumbnail_file(thumbnail_path, std::ios::binary);
  if (!thumbnail_file) {
    return std::unexpected("Failed to create thumbnail file: " + thumbnail_path.string());
  }

  thumbnail_file.write(reinterpret_cast<const char*>(webp_data.data.data()), webp_data.data.size());
  thumbnail_file.close();

  if (!thumbnail_file.good()) {
    return std::unexpected("Failed to write thumbnail data to file");
  }

  Logger().debug("Generated thumbnail: {} ({} bytes)", thumbnail_path.string(),
                 webp_data.data.size());
  return thumbnail_path;
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
  auto cache_result = Service::load_asset_cache(app_state);
  if (cache_result) {
    for (const auto& [path, metadata] : cache_result.value()) {
      if (!metadata.hash.empty()) {
        all_file_hashes.insert(metadata.hash);
      }
    }
  }

  std::int64_t total_size = 0;
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
      auto hash_result = extract_hash_from_thumbnail(entry.path());
      if (hash_result && all_file_hashes.find(*hash_result) == all_file_hashes.end()) {
        orphaned_thumbnails++;
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
