module;

module Features.Gallery;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Scanner;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Processor;
import Features.Gallery.Ignore.Processor;
import Features.Gallery.StaticResolver;
import Utils.Image;
import Utils.Logger;
import Utils.LRUCache;

namespace Features::Gallery {

// ============= 初始化和清理 =============

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    Logger().info("Initializing gallery module...");

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

    // 从数据库删除（软删除或硬删除）
    std::expected<void, std::string> delete_result;
    if (params.delete_file.value_or(false)) {
      delete_result = Asset::Repository::hard_delete_asset(app_state, params.id);
    } else {
      delete_result = Asset::Repository::soft_delete_asset(app_state, params.id);
    }

    Types::OperationResult result;
    if (delete_result) {
      result.success = true;
      result.message = params.delete_file.value_or(false)
                           ? "Asset item and file deleted successfully"
                           : "Asset item deleted successfully";
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

// ============= 扫描和索引 =============

auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string> {
  auto scan_result = Scanner::scan_asset_directory(app_state, options);
  if (!scan_result) {
    Logger().error("Asset scan failed: {}", scan_result.error());
    return std::unexpected("Asset scan failed: " + scan_result.error());
  }

  auto result = scan_result.value();
  Logger().info("Asset scan completed. Total: {}, New: {}, Updated: {}, Errors: {}",
                result.total_files, result.new_items, result.updated_items, result.errors.size());

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

// ============= 维护和优化 =============

auto cleanup_deleted_assets(Core::State::AppState& app_state, int days_old)
    -> std::expected<Types::OperationResult, std::string> {
  try {
    auto cleanup_result = Asset::Repository::cleanup_soft_deleted_assets(app_state, days_old);

    Types::OperationResult result;
    if (cleanup_result) {
      result.success = true;
      result.message = std::format("Cleaned up {} deleted items older than {} days",
                                   cleanup_result.value(), days_old);
      result.affected_count = cleanup_result.value();
      Logger().info("Database cleanup completed: {} items removed", cleanup_result.value());
    } else {
      result.success = false;
      result.message = "Failed to cleanup deleted items: " + cleanup_result.error();
      result.affected_count = 0;
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in cleanup_deleted_items: " + std::string(e.what()));
  }
}

}  // namespace Features::Gallery
