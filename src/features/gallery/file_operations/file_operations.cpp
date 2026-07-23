module;

module Features.Gallery.FileOperations;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Watcher;
import Utils.File;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.System;

namespace Features::Gallery::FileOperations {

// ============= 资产项管理 =============

// 按用户意图删除资产索引，并可选删除对应的物理文件。
auto delete_asset(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  try {
    // 获取要删除的资产项
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, params.id);
    if (!asset_result) {
      return std::unexpected("Failed to get asset item: " + asset_result.error());
    }

    if (!asset_result->has_value()) {
      Types::OperationResult result;
      result.success = false;
      result.message = "Asset item not found";
      result.affected_count = 0;
      return result;
    }

    auto asset = asset_result->value();

    // 删除物理文件（如果请求）
    if (params.delete_file.value_or(false)) {
      std::filesystem::path file_path(asset.path);
      if (std::filesystem::exists(file_path)) {
        std::error_code ec;
        std::filesystem::remove(file_path, ec);
        if (ec) {
          Logger().warn("Failed to delete physical file {}: {}", asset.path, ec.message());
        } else {
          Logger().info("Deleted physical file: {}", asset.path);
        }
      }
    }

    // 仅删除资产索引；按 hash 共享的缩略图由缓存对账确认无引用后统一回收。
    auto delete_result = Asset::Repository::delete_asset(app_state, params.id);

    Types::OperationResult result;
    if (delete_result) {
      result.success = true;
      result.message = params.delete_file.value_or(false)
                           ? "Asset item and file deleted successfully"
                           : "Asset item removed from library successfully";
      result.affected_count = 1;
    } else {
      result.success = false;
      result.message = "Failed to delete asset item: " + delete_result.error();
      result.affected_count = 0;
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in delete_asset: " + std::string(e.what()));
  }
}

// 使用系统默认应用打开指定资产文件。
auto open_asset_with_default_app(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string> {
  auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto open_result =
      Utils::System::open_file_with_default_app(std::filesystem::path(asset_result->value().path));
  if (!open_result) {
    return std::unexpected("Failed to open asset with default app: " + open_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Asset opened with default app",
      .affected_count = 0,
  };
}

// 在资源管理器中定位指定资产文件。
auto reveal_asset_in_explorer(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string> {
  auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto reveal_result =
      Utils::System::reveal_file_in_explorer(std::filesystem::path(asset_result->value().path));
  if (!reveal_result) {
    return std::unexpected("Failed to reveal asset in explorer: " + reveal_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Asset revealed in explorer",
      .affected_count = 0,
  };
}

// 将选中资产的物理文件移入回收站，并同步删除资产索引。
auto move_assets_to_trash(Core::State::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  struct TrashCandidate {
    Types::Asset asset;
    std::filesystem::path file_path;
    bool file_exists = false;
    bool manual_ignore_registered = false;
  };

  std::unordered_set<std::int64_t> unique_ids(ids.begin(), ids.end());
  std::vector<TrashCandidate> candidates;
  candidates.reserve(unique_ids.size());

  std::int64_t moved_count = 0;
  std::int64_t skipped_not_found = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());
  std::vector<Types::ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size());
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size());

  auto append_manual_change = [&](const std::filesystem::path& path,
                                  Types::ScanChangeAction action) {
    auto normalized_key = Utils::Path::NormalizeForComparison(path);
    auto action_key =
        action == Types::ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + Utils::String::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(Types::ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
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
    std::error_code ec;
    bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    candidates.push_back(TrashCandidate{
        .asset = asset,
        .file_path = std::move(file_path),
        .file_exists = file_exists,
    });
  }

  std::vector<std::filesystem::path> recycle_paths;
  recycle_paths.reserve(candidates.size());
  for (auto& candidate : candidates) {
    if (!candidate.file_exists) {
      continue;
    }

    auto begin_ignore_result = Watcher::begin_manual_file_system_ignore(
        app_state, candidate.file_path, candidate.file_path);
    if (!begin_ignore_result) {
      Logger().warn("Failed to register watcher ignore for recycle-bin move '{}': {}",
                    candidate.file_path.string(), begin_ignore_result.error());
    } else {
      candidate.manual_ignore_registered = true;
    }

    recycle_paths.push_back(candidate.file_path);
  }

  bool recycle_failed = false;
  if (!recycle_paths.empty()) {
    auto recycle_result = Utils::System::move_files_to_recycle_bin(recycle_paths);
    if (!recycle_result) {
      recycle_failed = true;
      errors.push_back("Failed to move files to recycle bin: " + recycle_result.error());
    }
  }

  std::unordered_set<std::int64_t> failed_recycle_ids;
  if (recycle_failed) {
    for (const auto& candidate : candidates) {
      if (!candidate.file_exists) {
        continue;
      }
      failed_recycle_ids.insert(candidate.asset.id);
      errors.push_back("Failed to move file to recycle bin " + candidate.asset.path);
    }
  }

  std::vector<std::int64_t> delete_ids;
  delete_ids.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (failed_recycle_ids.contains(candidate.asset.id)) {
      continue;
    }
    delete_ids.push_back(candidate.asset.id);
  }

  // 批量只删除资产索引，共享缩略图由缓存对账确认成为孤儿后统一回收。
  auto delete_result = Asset::Repository::batch_delete_assets_by_ids(app_state, delete_ids);
  if (!delete_result) {
    errors.push_back("Failed to delete asset indexes: " + delete_result.error());
  } else {
    moved_count = static_cast<std::int64_t>(delete_ids.size());
    Logger().info("Gallery move_assets_to_trash: moved {} asset file(s) to recycle bin",
                  moved_count);
    std::unordered_set<std::int64_t> deleted_id_set(delete_ids.begin(), delete_ids.end());
    for (const auto& candidate : candidates) {
      if (!deleted_id_set.contains(candidate.asset.id)) {
        continue;
      }
      append_manual_change(candidate.file_path, Types::ScanChangeAction::REMOVE);
    }

    if (!manual_changes.empty()) {
      Logger().info("Gallery move_assets_to_trash: dispatching {} manual scan change(s)",
                    manual_changes.size());
      auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, manual_changes);
      if (!dispatch_result) {
        Logger().warn("Failed to dispatch manual scan changes after trash move: {}",
                      dispatch_result.error());
      }
    }
  }

  for (auto& candidate : candidates) {
    if (!candidate.manual_ignore_registered) {
      continue;
    }
    if (auto complete_ignore_result = Watcher::complete_manual_file_system_ignore(
            app_state, candidate.file_path, candidate.file_path);
        !complete_ignore_result) {
      Logger().warn("Failed to complete watcher ignore for recycle-bin move '{}': {}",
                    candidate.file_path.string(), complete_ignore_result.error());
    }
  }

  Types::OperationResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = moved_count,
      .failed_count =
          static_cast<std::int64_t>(unique_ids.size()) - moved_count - skipped_not_found,
      .not_found_count = skipped_not_found,
      .unchanged_count = 0,
  };

  if (errors.empty()) {
    result.message = std::format("Moved {} asset(s) to recycle bin", moved_count);
    return result;
  }

  if (moved_count > 0) {
    result.message = std::format("Moved {} asset(s) to recycle bin, {} failed, {} not found",
                                 moved_count, errors.size(), skipped_not_found);
  } else {
    result.message = std::format("Failed to move assets to recycle bin: {} failed, {} not found",
                                 errors.size(), skipped_not_found);
  }

  for (const auto& error : errors) {
    Logger().warn("move_assets_to_trash: {}", error);
  }

  return result;
}

// 将选中资产移动到目标图库文件夹，并同步更新路径索引。
auto move_assets_to_folder(Core::State::AppState& app_state,
                           const Types::MoveAssetsToFolderParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  if (params.target_folder_id <= 0) {
    return Types::OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  auto target_folder_result =
      Folder::Repository::get_folder_by_id(app_state, params.target_folder_id);
  if (!target_folder_result) {
    return std::unexpected("Failed to query target folder: " + target_folder_result.error());
  }
  if (!target_folder_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto normalized_target_folder_result =
      Utils::Path::NormalizePath(std::filesystem::path(target_folder_result->value().path));
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
  std::vector<Types::ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size() * 2);
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size() * 2);

  auto append_manual_change = [&](const std::filesystem::path& path,
                                  Types::ScanChangeAction action) {
    auto normalized_key = Utils::Path::NormalizeForComparison(path);
    auto action_key =
        action == Types::ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + Utils::String::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(Types::ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }
    if (!asset_result->has_value()) {
      skipped_not_found++;
      continue;
    }

    auto asset = asset_result->value();
    auto normalized_source_result = Utils::Path::NormalizePath(std::filesystem::path(asset.path));
    if (!normalized_source_result) {
      errors.push_back("Failed to normalize source path for asset " + std::to_string(asset.id) +
                       ": " + normalized_source_result.error());
      continue;
    }

    auto source_path = normalized_source_result.value();
    auto destination_path = target_folder_path / source_path.filename();
    auto normalized_destination_result = Utils::Path::NormalizePath(destination_path);
    if (!normalized_destination_result) {
      errors.push_back("Failed to normalize destination path for asset " +
                       std::to_string(asset.id) + ": " + normalized_destination_result.error());
      continue;
    }

    auto normalized_destination_path = normalized_destination_result.value();
    // 目标与源一致时不报错，按“跳过项”处理，便于批量操作给出可理解反馈。
    if (Utils::Path::NormalizeForComparison(source_path) ==
        Utils::Path::NormalizeForComparison(normalized_destination_path)) {
      skipped_same_folder++;
      continue;
    }

    bool manual_ignore_registered = false;
    // 先注册 watcher ignore，再执行 move，避免 watcher 抢先对同一路径做重复分析。
    if (auto begin_ignore_result = Watcher::begin_manual_file_system_ignore(
            app_state, source_path, normalized_destination_path);
        begin_ignore_result) {
      manual_ignore_registered = true;
    } else {
      Logger().warn("Failed to register watcher ignore for manual move '{}': {}",
                    source_path.string(), begin_ignore_result.error());
    }

    auto complete_manual_ignore = [&]() {
      if (!manual_ignore_registered) {
        return;
      }
      // 无论成功还是失败都尝试完成 ignore，防止 in-flight 计数泄漏。
      if (auto complete_ignore_result = Watcher::complete_manual_file_system_ignore(
              app_state, source_path, normalized_destination_path);
          !complete_ignore_result) {
        Logger().warn("Failed to complete watcher ignore for manual move '{}': {}",
                      source_path.string(), complete_ignore_result.error());
      }
      manual_ignore_registered = false;
    };

    auto move_result =
        Utils::File::move_path_blocking(source_path, normalized_destination_path, false);
    if (!move_result) {
      complete_manual_ignore();
      errors.push_back("Failed to move asset " + std::to_string(asset.id) + ": " +
                       move_result.error());
      continue;
    }

    // 文件系统移动成功后再更新索引，保证 DB 记录与磁盘最终位置保持一致。
    auto update_result = Asset::Repository::update_asset_location(
        app_state, asset.id, normalized_destination_path.filename().string(),
        normalized_destination_path.generic_string(), params.target_folder_id);
    if (!update_result) {
      complete_manual_ignore();
      errors.push_back("Failed to update asset index " + std::to_string(asset.id) + ": " +
                       update_result.error());
      continue;
    }

    complete_manual_ignore();

    append_manual_change(source_path, Types::ScanChangeAction::REMOVE);
    append_manual_change(normalized_destination_path, Types::ScanChangeAction::UPSERT);
    moved_count++;
  }

  if (!manual_changes.empty()) {
    auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, manual_changes);
    if (!dispatch_result) {
      Logger().warn("Failed to dispatch manual scan changes after folder move: {}",
                    dispatch_result.error());
    }
  }

  Types::OperationResult result{
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

}  // namespace Features::Gallery::FileOperations
