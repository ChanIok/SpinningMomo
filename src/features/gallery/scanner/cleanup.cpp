module;

module Features.Gallery.Scanner.Cleanup;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Scanner.AssetPipeline;
import Features.Gallery.Folder.Repository;
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
  int missing_count = 0;
  std::vector<std::string> removed_paths;
};

// 对磁盘已消失的资产开启 missing 宽限期；已有 missing 记录不重复计数或重置时间。
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

    if (!existing_paths.contains(cached_path) && !metadata.missing_at.has_value()) {
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
    auto missing_result = AssetPipeline::mark_asset_missing_at_path(app_state, metadata.path);
    if (!missing_result) {
      Logger().warn("Failed to mark removed asset {} missing: {}", metadata.id,
                    missing_result.error());
      continue;
    }
    if (missing_result.value()) {
      result.missing_count++;
    }
  }

  if (result.missing_count > 0) {
    Logger().info("Marked {} removed assets missing under '{}'", result.missing_count, root_str);
  }

  return result;
}

// 把发现阶段的真实目录库存转成清理阶段使用的路径集合。
auto build_expected_folder_paths(const std::vector<std::filesystem::path>& folder_paths,
                                 const std::filesystem::path& normalized_scan_root)
    -> std::unordered_set<std::string> {
  std::unordered_set<std::string> expected_paths;
  expected_paths.reserve(folder_paths.size() + 1);
  for (const auto& folder_path : folder_paths) {
    expected_paths.insert(folder_path.string());
  }
  expected_paths.insert(normalized_scan_root.string());

  return expected_paths;
}

// 删除扫描根下不在真实目录库存中的 folder 记录（先深后浅）。
auto cleanup_missing_folders(Core::State::AppState& app_state,
                             const std::filesystem::path& normalized_scan_root,
                             const std::vector<std::filesystem::path>& folder_paths) -> int {
  auto all_folders_result = Folder::Repository::list_all_folders(app_state);
  if (!all_folders_result) {
    Logger().warn("Failed to list folders for cleanup: {}", all_folders_result.error());
    return 0;
  }

  auto root_str = normalized_scan_root.string();
  auto expected_paths = build_expected_folder_paths(folder_paths, normalized_scan_root);

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

// 对账阶段：缺失文件进入宽限期，目录库存仍按真实文件系统收敛。
auto run_cleanup_phase(Core::State::AppState& app_state,
                       const std::filesystem::path& normalized_scan_root,
                       const std::vector<Types::FileSystemInfo>& file_infos,
                       const std::vector<std::filesystem::path>& folder_paths,
                       const std::unordered_map<std::string, Types::Metadata>& asset_cache,
                       const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> CleanupPhaseResult {
  Progress::report_scan_progress(progress_callback, "cleanup", 0, 1, Progress::kCleanupPercent,
                                 "Reconciling deleted files");

  auto removed_assets_result =
      cleanup_removed_assets(app_state, normalized_scan_root, file_infos, asset_cache);
  [[maybe_unused]] int deleted_folders =
      cleanup_missing_folders(app_state, normalized_scan_root, folder_paths);
  return CleanupPhaseResult{
      .missing_items = removed_assets_result.missing_count,
      .removed_paths = std::move(removed_assets_result.removed_paths),
  };
}

}  // namespace Features::Gallery::Scanner::Cleanup
