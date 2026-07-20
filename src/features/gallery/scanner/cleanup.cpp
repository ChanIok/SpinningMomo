module;

module Features.Gallery.Scanner.Cleanup;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Utils.Logger;

namespace Features::Gallery::Scanner::Cleanup {

// 判断 candidate 是否在 root 下（含 root 自身；路径使用正斜杠规范）
auto is_path_under_root(const std::string& candidate_path, const std::string& root_path) -> bool {
  if (candidate_path.size() < root_path.size()) {
    return false;
  }

  if (!candidate_path.starts_with(root_path)) {
    return false;
  }

  if (candidate_path.size() == root_path.size()) {
    return true;
  }

  return candidate_path[root_path.size()] == '/';
}

struct CleanupRemovedAssetsResult {
  int deleted_count = 0;
  std::vector<std::string> removed_paths;
};

// 清理磁盘已删除但数据库仍存在的资产（含缩略图）
auto cleanup_removed_assets(Core::State::AppState& app_state,
                            const std::filesystem::path& normalized_scan_root,
                            const std::vector<Types::FileSystemInfo>& file_infos,
                            const std::unordered_map<std::string, Types::Metadata>& asset_cache)
    -> CleanupRemovedAssetsResult {
  auto root_str = normalized_scan_root.string();

  std::unordered_set<std::string> existing_paths;
  existing_paths.reserve(file_infos.size());
  for (const auto& file_info : file_infos) {
    existing_paths.insert(file_info.path.string());
  }

  std::vector<Types::Metadata> removed_assets;
  for (const auto& [cached_path, metadata] : asset_cache) {
    if (!is_path_under_root(cached_path, root_str)) {
      continue;
    }

    if (!existing_paths.contains(cached_path)) {
      // DB 有但磁盘没有
      removed_assets.push_back(metadata);
    }
  }

  CleanupRemovedAssetsResult result;
  result.removed_paths.reserve(removed_assets.size());
  for (const auto& metadata : removed_assets) {
    result.removed_paths.push_back(metadata.path);
  }

  for (const auto& metadata : removed_assets) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, metadata.id);
    if (asset_result && asset_result->has_value()) {
      auto thumbnail_result = Asset::Thumbnail::delete_thumbnail(app_state, asset_result->value());
      if (!thumbnail_result) {
        Logger().debug("Thumbnail cleanup skipped for removed asset {}: {}", metadata.id,
                       thumbnail_result.error());
      }
    }

    auto delete_result = Asset::Repository::delete_asset(app_state, metadata.id);
    if (!delete_result) {
      Logger().warn("Failed to delete removed asset {}: {}", metadata.id, delete_result.error());
      continue;
    }

    result.deleted_count++;
  }

  if (result.deleted_count > 0) {
    Logger().info("Deleted {} removed assets under '{}'", result.deleted_count, root_str);
  }

  return result;
}

// 由本次发现的文件反推仍应存在的 folder 路径集合
auto build_expected_folder_paths(const std::vector<Types::FileSystemInfo>& file_infos,
                                 const std::filesystem::path& normalized_scan_root)
    -> std::unordered_set<std::string> {
  std::vector<std::filesystem::path> file_paths;
  file_paths.reserve(file_infos.size());
  for (const auto& file_info : file_infos) {
    file_paths.push_back(file_info.path);
  }

  auto folder_paths =
      Folder::Service::extract_unique_folder_paths(file_paths, normalized_scan_root);
  std::unordered_set<std::string> expected_paths;
  expected_paths.reserve(folder_paths.size() + 1);
  for (const auto& folder_path : folder_paths) {
    expected_paths.insert(folder_path.string());
  }
  expected_paths.insert(normalized_scan_root.string());

  return expected_paths;
}

// 删除扫描根下已不再有效的 folder 记录（先深后浅）
auto cleanup_missing_folders(Core::State::AppState& app_state,
                             const std::filesystem::path& normalized_scan_root,
                             const std::vector<Types::FileSystemInfo>& file_infos) -> int {
  auto all_folders_result = Folder::Repository::list_all_folders(app_state);
  if (!all_folders_result) {
    Logger().warn("Failed to list folders for cleanup: {}", all_folders_result.error());
    return 0;
  }

  auto root_str = normalized_scan_root.string();
  auto expected_paths = build_expected_folder_paths(file_infos, normalized_scan_root);

  struct MissingFolder {
    std::int64_t id;
    std::string path;
  };

  std::vector<MissingFolder> missing_folders;
  for (const auto& folder : all_folders_result.value()) {
    if (folder.path == root_str) {
      continue;
    }

    if (!is_path_under_root(folder.path, root_str)) {
      continue;
    }

    if (!expected_paths.contains(folder.path)) {
      missing_folders.push_back(MissingFolder{.id = folder.id, .path = folder.path});
    }
  }

  // 先删更深的目录，减少父子删除冲突
  std::ranges::sort(missing_folders, [](const MissingFolder& a, const MissingFolder& b) {
    return a.path.size() > b.path.size();
  });

  int deleted_folders = 0;
  for (const auto& folder : missing_folders) {
    auto delete_result = Folder::Repository::delete_folder(app_state, folder.id);
    if (!delete_result) {
      Logger().debug("Skip folder cleanup for '{}' (id={}): {}", folder.path, folder.id,
                     delete_result.error());
      continue;
    }
    deleted_folders++;
  }

  if (deleted_folders > 0) {
    Logger().info("Deleted {} stale folders under '{}'", deleted_folders, root_str);
  }

  return deleted_folders;
}

// 清理阶段：盘库对账，删除 DB 有盘无的资产与失效 folder
auto run_cleanup_phase(Core::State::AppState& app_state,
                       const std::filesystem::path& normalized_scan_root,
                       const std::vector<Types::FileSystemInfo>& file_infos,
                       const std::unordered_map<std::string, Types::Metadata>& asset_cache,
                       const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> CleanupPhaseResult {
  Progress::report_scan_progress(progress_callback, "cleanup", 0, 1, Progress::kCleanupPercent,
                                 "Reconciling deleted files");

  auto removed_assets_result =
      cleanup_removed_assets(app_state, normalized_scan_root, file_infos, asset_cache);
  [[maybe_unused]] int deleted_folders =
      cleanup_missing_folders(app_state, normalized_scan_root, file_infos);
  return CleanupPhaseResult{
      .deleted_items = removed_assets_result.deleted_count,
      .removed_paths = std::move(removed_assets_result.removed_paths),
  };
}

}  // namespace Features::Gallery::Scanner::Cleanup
