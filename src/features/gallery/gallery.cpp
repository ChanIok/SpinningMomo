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

auto move_assets_to_trash(Core::State::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  std::unordered_set<std::int64_t> unique_ids(ids.begin(), ids.end());
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

    if (file_exists) {
      auto recycle_result = Utils::System::move_files_to_recycle_bin({file_path});
      if (!recycle_result) {
        errors.push_back("Failed to move file to recycle bin " + asset.path + ": " +
                         recycle_result.error());
        continue;
      }
    }

    if (auto delete_thumbnail_result = Asset::Thumbnail::delete_thumbnail(app_state, asset);
        !delete_thumbnail_result) {
      Logger().warn("Failed to delete thumbnail for asset {}: {}", asset.id,
                    delete_thumbnail_result.error());
    }

    auto delete_result = Asset::Repository::delete_asset(app_state, asset.id);
    if (!delete_result) {
      errors.push_back("Failed to delete asset index " + std::to_string(asset.id) + ": " +
                       delete_result.error());
      continue;
    }

    moved_count++;
  }

  Types::OperationResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = moved_count,
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
