#include "features/gallery/file_operations/file_operations.hpp"

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/asset/repository.hpp"
#include "features/gallery/folder/repository.hpp"
#include "features/gallery/types.hpp"
#include "features/gallery/watcher/watcher.hpp"
#include "utils/file/file.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"
#include "utils/system/system.hpp"

namespace features::gallery::file_operations {

// 使用系统默认应用打开指定资产文件。
auto open_asset_with_default_app(core::AppState& app_state, std::int64_t id)
    -> std::expected<OperationResult, std::string> {
  auto asset_result = asset::repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto open_result =
      utils::system::open_file_with_default_app(std::filesystem::path(asset_result->value().path));
  if (!open_result) {
    return std::unexpected("Failed to open asset with default app: " + open_result.error());
  }

  return OperationResult{
      .success = true,
      .message = "Asset opened with default app",
      .affected_count = 0,
  };
}

// 在资源管理器中定位指定资产文件。
auto reveal_asset_in_explorer(core::AppState& app_state, std::int64_t id)
    -> std::expected<OperationResult, std::string> {
  auto asset_result = asset::repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto reveal_result =
      utils::system::reveal_file_in_explorer(std::filesystem::path(asset_result->value().path));
  if (!reveal_result) {
    return std::unexpected("Failed to reveal asset in explorer: " + reveal_result.error());
  }

  return OperationResult{
      .success = true,
      .message = "Asset revealed in explorer",
      .affected_count = 0,
  };
}

// 按明确策略删除资产；确认交互由前端统一负责。
auto delete_assets(core::AppState& app_state, const DeleteAssetsParams& params)
    -> std::expected<DeleteAssetsResult, std::string> {
  if (params.ids.empty()) {
    return DeleteAssetsResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  const bool permanent_mode = params.mode == "permanent";
  if (!permanent_mode && params.mode != "recycle_where_possible") {
    return std::unexpected("Invalid delete mode: " + params.mode);
  }

  struct DeleteCandidate {
    Asset asset;
    std::filesystem::path file_path;
    bool file_exists = false;
    bool permanent = false;
    bool manual_ignore_registered = false;
  };

  std::unordered_set<std::int64_t> unique_ids(params.ids.begin(), params.ids.end());
  std::vector<DeleteCandidate> candidates;
  candidates.reserve(unique_ids.size());

  std::int64_t skipped_not_found = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());
  std::vector<ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size());
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size());

  auto append_manual_change = [&](const std::filesystem::path& path, ScanChangeAction action) {
    auto normalized_key = utils::path::NormalizeForComparison(path);
    auto action_key =
        action == ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + utils::string::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = asset::repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }

    if (!asset_result->has_value()) {
      skipped_not_found++;
      continue;
    }

    const auto& asset = asset_result->value();
    std::filesystem::path file_path(asset.path);
    const bool permanent = permanent_mode || utils::path::ClassifyPathStorageKind(file_path) ==
                                                 utils::path::PathStorageKind::RemoteUnc;
    std::error_code ec;
    bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    candidates.push_back(DeleteCandidate{
        .asset = asset,
        .file_path = std::move(file_path),
        .file_exists = file_exists,
        .permanent = permanent,
    });
  }

  std::unordered_set<std::int64_t> failed_file_ids;
  std::vector<std::filesystem::path> recycle_paths;
  recycle_paths.reserve(candidates.size());
  for (auto& candidate : candidates) {
    if (!candidate.file_exists) {
      continue;
    }

    auto begin_ignore_result = watcher::begin_manual_file_system_ignore(
        app_state, candidate.file_path, candidate.file_path);
    if (!begin_ignore_result) {
      failed_file_ids.insert(candidate.asset.id);
      errors.push_back("Failed to register watcher ignore for asset deletion '" +
                       candidate.file_path.string() + "': " + begin_ignore_result.error());
      continue;
    }
    candidate.manual_ignore_registered = true;

    if (!candidate.permanent) {
      recycle_paths.push_back(candidate.file_path);
    }
  }

  if (!recycle_paths.empty()) {
    auto recycle_result = utils::system::move_files_to_recycle_bin(recycle_paths);
    if (!recycle_result) {
      errors.push_back("Failed to move files to recycle bin: " + recycle_result.error());
      for (const auto& candidate : candidates) {
        if (!candidate.permanent && candidate.file_exists) {
          failed_file_ids.insert(candidate.asset.id);
          errors.push_back("Failed to move file to recycle bin " + candidate.asset.path);
        }
      }
    }
  }

  for (const auto& candidate : candidates) {
    if (!candidate.permanent || !candidate.file_exists ||
        failed_file_ids.contains(candidate.asset.id)) {
      continue;
    }
    auto delete_file_result = utils::system::delete_file_permanently(candidate.file_path);
    if (!delete_file_result) {
      failed_file_ids.insert(candidate.asset.id);
      errors.push_back("Failed to permanently delete file " + candidate.asset.path + ": " +
                       delete_file_result.error());
    }
  }

  std::vector<std::int64_t> delete_ids;
  delete_ids.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (failed_file_ids.contains(candidate.asset.id)) {
      continue;
    }
    delete_ids.push_back(candidate.asset.id);
  }

  std::int64_t affected_count = 0;
  std::int64_t recycled_count = 0;
  std::int64_t permanently_deleted_count = 0;

  // 批量只删除资产索引，共享缩略图由缓存对账确认成为孤儿后统一回收。
  auto delete_result = asset::repository::batch_delete_assets_by_ids(app_state, delete_ids);
  if (!delete_result) {
    errors.push_back("Failed to delete asset indexes: " + delete_result.error());
  } else {
    affected_count = static_cast<std::int64_t>(delete_ids.size());
    std::unordered_set<std::int64_t> deleted_id_set(delete_ids.begin(), delete_ids.end());
    for (const auto& candidate : candidates) {
      if (!deleted_id_set.contains(candidate.asset.id)) {
        continue;
      }
      if (candidate.permanent) {
        permanently_deleted_count++;
      } else {
        recycled_count++;
      }
      append_manual_change(candidate.file_path, ScanChangeAction::REMOVE);
    }
  }

  for (auto& candidate : candidates) {
    if (!candidate.manual_ignore_registered) {
      continue;
    }
    if (auto complete_ignore_result = watcher::complete_manual_file_system_ignore(
            app_state, candidate.file_path, candidate.file_path);
        !complete_ignore_result) {
      Logger().warn("Failed to complete watcher ignore for asset deletion '{}': {}",
                    candidate.file_path.string(), complete_ignore_result.error());
    }
  }

  if (!manual_changes.empty()) {
    Logger().info("Gallery delete_assets: dispatching {} manual scan change(s)",
                  manual_changes.size());
    auto dispatch_result = watcher::dispatch_manual_scan_changes(app_state, manual_changes);
    if (!dispatch_result) {
      Logger().warn("Failed to dispatch manual scan changes after asset deletion: {}",
                    dispatch_result.error());
    }
  }

  DeleteAssetsResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = affected_count,
      .failed_count =
          static_cast<std::int64_t>(unique_ids.size()) - affected_count - skipped_not_found,
      .not_found_count = skipped_not_found,
      .unchanged_count = 0,
      .recycle_bin_count = recycled_count,
      .permanent_count = permanently_deleted_count,
  };

  if (errors.empty()) {
    result.message = std::format("Deleted {} asset(s): {} recycled, {} permanently deleted",
                                 affected_count, recycled_count, permanently_deleted_count);
    return result;
  }

  if (affected_count > 0) {
    result.message = std::format("Deleted {} asset(s), {} failed, {} not found", affected_count,
                                 result.failed_count.value_or(0), skipped_not_found);
  } else {
    result.message = std::format("Failed to delete assets: {} failed, {} not found",
                                 result.failed_count.value_or(0), skipped_not_found);
  }

  for (const auto& error : errors) {
    Logger().warn("delete_assets: {}", error);
  }

  return result;
}

// 将选中资产移动到目标图库文件夹，并同步更新路径索引。
auto move_assets_to_folder(core::AppState& app_state, const MoveAssetsToFolderParams& params)
    -> std::expected<OperationResult, std::string> {
  if (params.ids.empty()) {
    return OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  if (params.target_folder_id <= 0) {
    return OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  auto target_folder_result =
      folder::repository::get_folder_by_id(app_state, params.target_folder_id);
  if (!target_folder_result) {
    return std::unexpected("Failed to query target folder: " + target_folder_result.error());
  }
  if (!target_folder_result->has_value()) {
    return OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto normalized_target_folder_result =
      utils::path::NormalizePath(std::filesystem::path(target_folder_result->value().path));
  if (!normalized_target_folder_result) {
    return std::unexpected("Failed to normalize target folder path: " +
                           normalized_target_folder_result.error());
  }

  auto target_folder_path = normalized_target_folder_result.value();
  std::unordered_set<std::int64_t> unique_ids(params.ids.begin(), params.ids.end());
  std::int64_t moved_count = 0;
  std::int64_t skipped_not_found = 0;
  std::int64_t skipped_same_folder = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());
  std::vector<ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size() * 2);
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size() * 2);

  auto append_manual_change = [&](const std::filesystem::path& path, ScanChangeAction action) {
    auto normalized_key = utils::path::NormalizeForComparison(path);
    auto action_key =
        action == ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + utils::string::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = asset::repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }
    if (!asset_result->has_value()) {
      skipped_not_found++;
      continue;
    }

    auto asset = asset_result->value();
    auto normalized_source_result = utils::path::NormalizePath(std::filesystem::path(asset.path));
    if (!normalized_source_result) {
      errors.push_back("Failed to normalize source path for asset " + std::to_string(asset.id) +
                       ": " + normalized_source_result.error());
      continue;
    }

    auto source_path = normalized_source_result.value();
    auto destination_path = target_folder_path / source_path.filename();
    auto normalized_destination_result = utils::path::NormalizePath(destination_path);
    if (!normalized_destination_result) {
      errors.push_back("Failed to normalize destination path for asset " +
                       std::to_string(asset.id) + ": " + normalized_destination_result.error());
      continue;
    }

    auto normalized_destination_path = normalized_destination_result.value();
    // 目标与源一致时不报错，按“跳过项”处理，便于批量操作给出可理解反馈。
    if (utils::path::NormalizeForComparison(source_path) ==
        utils::path::NormalizeForComparison(normalized_destination_path)) {
      skipped_same_folder++;
      continue;
    }

    // 先注册 watcher ignore，再执行 move，避免 watcher 抢先对同一路径做重复分析。
    auto begin_ignore_result = watcher::begin_manual_file_system_ignore(
        app_state, source_path, normalized_destination_path);
    if (!begin_ignore_result) {
      errors.push_back("Failed to register watcher ignore for manual move '" +
                       source_path.string() + "': " + begin_ignore_result.error());
      continue;
    }

    auto complete_manual_ignore = [&]() {
      if (auto complete_ignore_result = watcher::complete_manual_file_system_ignore(
              app_state, source_path, normalized_destination_path);
          !complete_ignore_result) {
        Logger().warn("Failed to complete watcher ignore for manual move '{}': {}",
                      source_path.string(), complete_ignore_result.error());
      }
    };

    auto move_result =
        utils::file::move_path_blocking(source_path, normalized_destination_path, false);
    if (!move_result) {
      complete_manual_ignore();
      errors.push_back("Failed to move asset " + std::to_string(asset.id) + ": " +
                       move_result.error());
      continue;
    }

    // 文件系统移动成功后再更新索引，保证 DB 记录与磁盘最终位置保持一致。
    auto update_result = asset::repository::update_asset_location(
        app_state, asset.id, normalized_destination_path.filename().string(),
        normalized_destination_path.generic_string(), params.target_folder_id);
    if (!update_result) {
      complete_manual_ignore();
      errors.push_back("Failed to update asset index " + std::to_string(asset.id) + ": " +
                       update_result.error());
      continue;
    }

    complete_manual_ignore();

    append_manual_change(source_path, ScanChangeAction::REMOVE);
    append_manual_change(normalized_destination_path, ScanChangeAction::UPSERT);
    moved_count++;
  }

  if (!manual_changes.empty()) {
    auto dispatch_result = watcher::dispatch_manual_scan_changes(app_state, manual_changes);
    if (!dispatch_result) {
      Logger().warn("Failed to dispatch manual scan changes after folder move: {}",
                    dispatch_result.error());
    }
  }

  OperationResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = moved_count,
      .failed_count = static_cast<std::int64_t>(unique_ids.size()) - moved_count -
                      skipped_not_found - skipped_same_folder,
      .not_found_count = skipped_not_found,
      .unchanged_count = skipped_same_folder,
  };

  if (errors.empty()) {
    result.message = std::format("Moved {} asset(s) to target folder", moved_count);
    return result;
  }

  if (moved_count > 0) {
    result.message =
        std::format("Moved {} asset(s), {} failed, {} not found, {} already in target folder",
                    moved_count, errors.size(), skipped_not_found, skipped_same_folder);
  } else {
    result.message = std::format("Failed to move assets: {} failed, {} not found, {} unchanged",
                                 errors.size(), skipped_not_found, skipped_same_folder);
  }

  for (const auto& error : errors) {
    Logger().warn("move_assets_to_folder: {}", error);
  }

  return result;
}

}  // namespace features::gallery::file_operations
