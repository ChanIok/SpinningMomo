module;

#include <mfapi.h>
#include <asio.hpp>

module Features.Gallery;

import std;
import Core.Async;
import Core.RPC.NotificationHub;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Service;
import Features.Gallery.Scanner;
import Features.Gallery.ScanCommon;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Service;
import Features.Gallery.StaticResolver;
import Features.Gallery.Watcher;
import Utils.File;
import Utils.Image;
import Utils.Logger;
import Utils.LRUCache;
import Utils.Path;
import Utils.System;

namespace Features::Gallery {

auto make_bootstrap_scan_options(const std::filesystem::path& directory) -> Types::ScanOptions {
  Types::ScanOptions options;
  options.directory = directory.string();
  options.supported_extensions = ScanCommon::default_supported_extensions();
  return options;
}

auto ensure_output_directory_media_source(Core::State::AppState& app_state,
                                          const std::string& output_dir_path) -> void {
  if (!app_state.async) {
    Logger().warn("Skip output-directory gallery sync: async state is not ready");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Logger().warn("Skip output-directory gallery sync: async runtime is not available");
    return;
  }

  auto output_dir_path_snapshot = output_dir_path;
  asio::co_spawn(
      *io_context,
      [&app_state, output_dir_path_snapshot]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);

        auto output_dir_result = Utils::Path::GetOutputDirectory(output_dir_path_snapshot);
        if (!output_dir_result) {
          Logger().warn("Failed to resolve output directory for gallery sync: {}",
                        output_dir_result.error());
        } else {
          auto scan_result =
              scan_directory(app_state, make_bootstrap_scan_options(output_dir_result.value()));
          if (!scan_result) {
            Logger().warn("Failed to scan output directory for gallery sync '{}': {}",
                          output_dir_result->string(), scan_result.error());
          } else {
            Logger().info("Output directory added to gallery sources: {}",
                          output_dir_result->string());
            Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
          }
        }
      },
      asio::detached);
}

// ============= 初始化和清理 =============

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    Logger().info("Initializing gallery module...");

    // 供 Utils::Media::VideoAsset（SourceReader）使用；须在任意 analyze 之前成功。
    if (FAILED(MFStartup(MF_VERSION))) {
      return std::unexpected("Failed to initialize Media Foundation for gallery");
    }

    // 确保缩略图目录存在
    auto ensure_dir_result = Asset::Thumbnail::ensure_thumbnails_directory_exists(app_state);
    if (!ensure_dir_result) {
      Logger().error("Failed to ensure thumbnails directory exists: {}", ensure_dir_result.error());
      return std::unexpected("Failed to ensure thumbnails directory exists: " +
                             ensure_dir_result.error());
    }

    // 注册静态服务解析器
    StaticResolver::register_http_resolvers(app_state);
    StaticResolver::register_webview_resolvers(app_state);

    // 根据数据库里的根文件夹记录，确保 WebView 原图 host mappings 全部就绪。
    if (auto mapping_result = Folder::Service::ensure_all_root_folder_webview_mappings(app_state);
        !mapping_result) {
      return std::unexpected("Failed to sync gallery root WebView mappings: " +
                             mapping_result.error());
    }

    Logger().info("Gallery module initialized successfully");
    Logger().info("Thumbnail directory set to: {}",
                  app_state.gallery->thumbnails_directory.string());
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during asset module initialization: " +
                           std::string(e.what()));
  }
}

auto cleanup(Core::State::AppState& app_state) -> void {
  try {
    Logger().info("Cleaning up gallery module resources...");

    // 注销静态服务解析器
    StaticResolver::unregister_all_resolvers(app_state);

    // 重置缩略图路径状态
    app_state.gallery->thumbnails_directory.clear();

    // 与 initialize 中 MFStartup 成对；此后不应再调用视频分析。
    MFShutdown();

    Logger().info("Gallery module cleanup completed");
  } catch (const std::exception& e) {
    Logger().error("Exception during asset module cleanup: {}", e.what());
  }
}

// ============= 资产项管理 =============

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

    // 删除缩略图
    auto delete_thumbnail_result = Asset::Thumbnail::delete_thumbnail(app_state, asset);
    if (!delete_thumbnail_result) {
      Logger().warn("Failed to delete thumbnail for asset item {}: {}", params.id,
                    delete_thumbnail_result.error());
    }

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

    // 从数据库删除
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

auto copy_assets_to_clipboard(Core::State::AppState& app_state,
                              const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  // 这一层负责把“选中的资产 ID”转换成真正可复制的磁盘文件路径。
  // 真正的系统剪贴板写入由 Utils::System 负责。
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  std::vector<std::int64_t> unique_ids;
  unique_ids.reserve(ids.size());
  std::unordered_set<std::int64_t> seen_ids;
  seen_ids.reserve(ids.size());

  // 保持选择集顺序，同时去掉重复 ID，避免重复复制同一个文件。
  for (auto id : ids) {
    if (seen_ids.insert(id).second) {
      unique_ids.push_back(id);
    }
  }

  std::vector<std::filesystem::path> clipboard_paths;
  clipboard_paths.reserve(unique_ids.size());

  std::int64_t copied_count = 0;
  std::int64_t not_found_count = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());

  // 逐个把资产 ID 转成文件路径，并过滤掉索引不存在或磁盘不存在的项。
  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }

    if (!asset_result->has_value()) {
      not_found_count++;
      continue;
    }

    const auto& asset = asset_result->value();
    if (asset.path.empty()) {
      errors.push_back("Asset path is empty for asset " + std::to_string(asset.id));
      continue;
    }

    std::filesystem::path file_path(asset.path);
    std::error_code ec;
    const bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    if (!file_exists) {
      not_found_count++;
      continue;
    }

    clipboard_paths.push_back(std::move(file_path));
  }

  // 只有在至少找到一个真实文件时，才真正写入系统剪贴板。
  if (!clipboard_paths.empty()) {
    auto copy_result = Utils::System::copy_files_to_clipboard(clipboard_paths);
    if (!copy_result) {
      errors.push_back("Failed to copy files to clipboard: " + copy_result.error());
    } else {
      copied_count = static_cast<std::int64_t>(clipboard_paths.size());
    }
  }

  const auto total_count = static_cast<std::int64_t>(unique_ids.size());
  const auto failed_count = std::max<std::int64_t>(0, total_count - copied_count - not_found_count);

  // 这里沿用 gallery 现有的 OperationResult 风格，
  // 方便前端统一做 success / partial / failed 的 toast 提示。
  Types::OperationResult result{
      .success = copied_count == total_count,
      .message = "",
      .affected_count = copied_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = 0,
  };

  if (result.success) {
    result.message = std::format("Copied {} asset(s) to clipboard", copied_count);
    return result;
  }

  if (copied_count > 0) {
    result.message = std::format("Copied {} asset(s) to clipboard, {} failed, {} not found",
                                 copied_count, failed_count, not_found_count);
  } else {
    result.message = std::format("Failed to copy assets to clipboard: {} failed, {} not found",
                                 failed_count, not_found_count);
  }

  // 详细错误记日志，用户界面只展示汇总结果即可。
  for (const auto& error : errors) {
    Logger().warn("copy_assets_to_clipboard: {}", error);
  }

  return result;
}

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

    auto begin_ignore_result =
        Watcher::begin_manual_move_ignore(app_state, candidate.file_path, candidate.file_path);
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
    if (auto delete_thumbnail_result =
            Asset::Thumbnail::delete_thumbnail(app_state, candidate.asset);
        !delete_thumbnail_result) {
      Logger().warn("Failed to delete thumbnail for asset {}: {}", candidate.asset.id,
                    delete_thumbnail_result.error());
    }
    delete_ids.push_back(candidate.asset.id);
  }

  auto delete_result = Asset::Repository::batch_delete_assets_by_ids(app_state, delete_ids);
  if (!delete_result) {
    errors.push_back("Failed to delete asset indexes: " + delete_result.error());
  } else {
    moved_count = static_cast<std::int64_t>(delete_ids.size());
  }

  for (auto& candidate : candidates) {
    if (!candidate.manual_ignore_registered) {
      continue;
    }
    if (auto complete_ignore_result = Watcher::complete_manual_move_ignore(
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
    if (auto begin_ignore_result =
            Watcher::begin_manual_move_ignore(app_state, source_path, normalized_destination_path);
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
      if (auto complete_ignore_result = Watcher::complete_manual_move_ignore(
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

    asset.path = normalized_destination_path.generic_string();
    asset.name = normalized_destination_path.filename().string();
    asset.folder_id = params.target_folder_id;
    // 文件系统移动成功后再更新索引，保证 DB 记录与磁盘最终位置保持一致。
    auto update_result = Asset::Repository::update_asset(app_state, asset);
    if (!update_result) {
      complete_manual_ignore();
      errors.push_back("Failed to update asset index " + std::to_string(asset.id) + ": " +
                       update_result.error());
      continue;
    }

    complete_manual_ignore();

    moved_count++;
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

// ============= 扫描和索引 =============

auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                    std::function<void(const Types::ScanProgress&)> progress_callback)
    -> std::expected<Types::ScanResult, std::string> {
  auto scan_result =
      Scanner::scan_asset_directory(app_state, options, std::move(progress_callback));
  if (!scan_result) {
    Logger().error("Asset scan failed: {}", scan_result.error());
    return std::unexpected("Asset scan failed: " + scan_result.error());
  }

  auto result = scan_result.value();
  Logger().info("Asset scan completed. Total: {}, New: {}, Updated: {}, Errors: {}",
                result.total_files, result.new_items, result.updated_items, result.errors.size());

  auto watcher_result = Watcher::register_watcher_for_directory(
      app_state, std::filesystem::path(options.directory), options);
  if (!watcher_result) {
    // 扫描已经成功，监听失败这里只记日志，不中断流程。
    Logger().warn("Failed to ensure watcher for '{}': {}", options.directory,
                  watcher_result.error());
    return result;
  }

  auto start_result = Watcher::start_watcher_for_directory(
      app_state, std::filesystem::path(options.directory), false);
  if (!start_result) {
    Logger().warn("Failed to start watcher for '{}': {}", options.directory, start_result.error());
  }

  return result;
}

auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string> {
  try {
    auto cleanup_result = Asset::Thumbnail::cleanup_orphaned_thumbnails(app_state);

    Types::OperationResult result;
    if (cleanup_result) {
      result.success = true;
      result.message = std::format("Cleaned up {} orphaned thumbnails", cleanup_result.value());
      result.affected_count = cleanup_result.value();
      Logger().info("Thumbnail cleanup completed: {} files removed", cleanup_result.value());
    } else {
      result.success = false;
      result.message = "Failed to cleanup thumbnails: " + cleanup_result.error();
      result.affected_count = 0;
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in cleanup_thumbnails: " + std::string(e.what()));
  }
}

// ============= 统计和信息 =============

auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string> {
  try {
    auto stats_result = Asset::Thumbnail::get_thumbnail_stats(app_state);
    if (!stats_result) {
      return std::unexpected(stats_result.error());
    }

    auto stats = stats_result.value();

    std::string formatted_stats = std::format(
        "Thumbnail Statistics:\\n"
        "Directory: {}\\n"
        "Total Thumbnails: {}\\n"
        "Total Size: {} bytes\\n"
        "Orphaned Thumbnails: {}\\n"
        "Corrupted Thumbnails: {}",
        stats.thumbnails_directory, stats.total_thumbnails, stats.total_size,
        stats.orphaned_thumbnails, stats.corrupted_thumbnails);

    return formatted_stats;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_stats: " + std::string(e.what()));
  }
}

}  // namespace Features::Gallery
