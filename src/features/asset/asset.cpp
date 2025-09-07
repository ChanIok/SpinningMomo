module;

#include <format>

module Features.Asset;

import std;
import Core.State;
import Features.Asset.State;
import Features.Asset.Types;
import Features.Asset.Repository;
import Features.Asset.Scanner;
import Features.Asset.Thumbnail;
import Utils.Image;
import Utils.Logger;

namespace Features::Asset {

// ============= 初始化和清理 =============

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    Logger().info("Initializing asset module...");

    // 确保缩略图目录存在
    auto ensure_dir_result = Thumbnail::ensure_thumbnails_directory_exists(app_state);
    if (!ensure_dir_result) {
      Logger().error("Failed to ensure thumbnails directory exists: {}", ensure_dir_result.error());
      return std::unexpected("Failed to ensure thumbnails directory exists: " +
                             ensure_dir_result.error());
    }
    
    Logger().info("Asset module initialized successfully");
    Logger().info("Thumbnail directory set to: {}", app_state.asset->thumbnails_directory.string());
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during asset module initialization: " +
                           std::string(e.what()));
  }
}

auto cleanup(Core::State::AppState& app_state) -> void {
  try {
    Logger().info("Cleaning up asset module resources...");
    
    // 重置缩略图路径状态
    app_state.asset->thumbnails_directory.clear();
    
    Logger().info("Asset module cleanup completed");
  } catch (const std::exception& e) {
    Logger().error("Exception during asset module cleanup: {}", e.what());
  }
}

// ============= 资产项管理 =============

auto delete_asset_item(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::AssetOperationResult, std::string> {
  try {
    // 获取要删除的资产项
    auto asset_item_result = Repository::get_asset_by_id(app_state, params.id);
    if (!asset_item_result) {
      return std::unexpected("Failed to get asset item: " + asset_item_result.error());
    }

    if (!asset_item_result->has_value()) {
      Types::AssetOperationResult result;
      result.success = false;
      result.message = "Asset item not found";
      result.affected_count = 0;
      return result;
    }

    auto asset_item = asset_item_result->value();

    // 删除缩略图
    auto delete_thumbnail_result = Thumbnail::delete_thumbnail(app_state, asset_item);
    if (!delete_thumbnail_result) {
      Logger().warn("Failed to delete thumbnail for asset item {}: {}", params.id,
                    delete_thumbnail_result.error());
    }

    // 删除物理文件（如果请求）
    if (params.delete_file.value_or(false)) {
      std::filesystem::path file_path(asset_item.filepath);
      if (std::filesystem::exists(file_path)) {
        std::error_code ec;
        std::filesystem::remove(file_path, ec);
        if (ec) {
          Logger().warn("Failed to delete physical file {}: {}", asset_item.filepath, ec.message());
        } else {
          Logger().info("Deleted physical file: {}", asset_item.filepath);
        }
      }
    }

    // 从数据库删除（软删除或硬删除）
    std::expected<void, std::string> delete_result;
    if (params.delete_file.value_or(false)) {
      delete_result = Repository::hard_delete_asset(app_state, params.id);
    } else {
      delete_result = Repository::soft_delete_asset(app_state, params.id);
    }

    Types::AssetOperationResult result;
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
    return std::unexpected("Exception in delete_asset_item: " + std::string(e.what()));
  }
}

// ============= 扫描和索引 =============

auto scan_asset_directories(Core::State::AppState& app_state, const Types::ScanParams& params)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    // 验证参数
    auto validation_result = validate_asset_scan_params(params);
    if (!validation_result) {
      return std::unexpected(validation_result.error());
    }

    // 转换参数为 Scanner 需要的格式
    Types::AssetScanOptions scan_options;
    scan_options.directories = params.directories;
    scan_options.recursive = params.recursive.value_or(true);
    scan_options.generate_thumbnails = params.generate_thumbnails.value_or(true);
    scan_options.thumbnail_max_width = params.thumbnail_max_width.value_or(400);
    scan_options.thumbnail_max_height = params.thumbnail_max_height.value_or(400);

    Logger().info("Starting asset scan of {} directories", scan_options.directories.size());

    // 执行扫描
    auto scan_result = Scanner::scan_asset_directory(app_state, scan_options);
    if (!scan_result) {
      Logger().error("Asset scan failed: {}", scan_result.error());
      return std::unexpected("Scan failed: " + scan_result.error());
    }

    auto result = scan_result.value();
    Logger().info("Asset scan completed. New: {}, Updated: {}, Errors: {}", result.new_items,
                  result.updated_items, result.errors.size());

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in scan_asset_directories: " + std::string(e.what()));
  }
}

auto rescan_asset_directory(Core::State::AppState& app_state, const std::string& directory_path,
                            bool generate_thumbnails)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    Types::ScanParams params;
    params.directories = {directory_path};
    params.recursive = true;
    params.generate_thumbnails = generate_thumbnails;

    return scan_asset_directories(app_state, params);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in rescan_asset_directory: " + std::string(e.what()));
  }
}

// ============= 缩略图管理 =============

auto get_asset_thumbnail_data(Core::State::AppState& app_state,
                              const Types::GetThumbnailParams& params)
    -> std::expected<std::vector<uint8_t>, std::string> {
  return Thumbnail::get_asset_thumbnail_data(app_state, params.asset_id);
}

auto generate_missing_asset_thumbnails(Core::State::AppState& app_state, uint32_t max_width,
                                       uint32_t max_height)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    Logger().info("Starting generation of missing thumbnails...");

    // 创建 WIC 工厂
    auto wic_factory_result = Utils::Image::create_factory();
    if (!wic_factory_result) {
      return std::unexpected("Failed to create WIC factory: " + wic_factory_result.error());
    }
    auto wic_factory = wic_factory_result.value();

    // 获取所有照片类型的资产项
    Types::ListParams list_params;
    list_params.filter_type = "photo";
    list_params.per_page = 1000;  // 批量处理

    auto asset_list_result = Repository::list_asset(app_state, list_params);
    if (!asset_list_result) {
      return std::unexpected("Failed to get asset list: " + asset_list_result.error());
    }

    auto asset_list = asset_list_result.value();
    std::vector<Types::Asset> items_needing_thumbnails;

    // 检查哪些项目需要生成缩略图
    for (const auto& item : asset_list.items) {
      auto needs_regen_result = Thumbnail::needs_asset_thumbnail_regeneration(item);
      if (needs_regen_result && needs_regen_result.value()) {
        items_needing_thumbnails.push_back(item);
      }
    }

    Logger().info("Found {} items needing thumbnail generation", items_needing_thumbnails.size());

    // 批量生成缩略图
    auto thumbnails_result = Thumbnail::batch_generate_asset_thumbnails(
        app_state, wic_factory, items_needing_thumbnails, max_width, max_height);

    Types::AssetScanResult result;
    if (thumbnails_result) {
      result.total_files = static_cast<int>(items_needing_thumbnails.size());
      result.new_items = static_cast<int>(thumbnails_result->size());
      result.updated_items = 0;
      result.deleted_items = 0;
      result.scan_duration = "N/A";
    } else {
      result.total_files = static_cast<int>(items_needing_thumbnails.size());
      result.new_items = 0;
      result.updated_items = 0;
      result.deleted_items = 0;
      result.errors.push_back("Failed to generate thumbnails: " + thumbnails_result.error());
    }

    Logger().info("Thumbnail generation completed. Generated: {}", result.new_items);

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in generate_missing_thumbnails: " + std::string(e.what()));
  }
}

auto cleanup_asset_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::AssetOperationResult, std::string> {
  try {
    auto cleanup_result = Thumbnail::cleanup_orphaned_thumbnails(app_state);

    Types::AssetOperationResult result;
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

auto get_asset_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::AssetStats, std::string> {
  return Repository::get_asset_stats(app_state, params);
}

auto get_asset_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string> {
  try {
    auto stats_result = Thumbnail::get_asset_thumbnail_stats(app_state);
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
    -> std::expected<Types::AssetOperationResult, std::string> {
  try {
    auto cleanup_result = Repository::cleanup_soft_deleted_assets(app_state, days_old);

    Types::AssetOperationResult result;
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

auto verify_asset_integrity(Core::State::AppState& app_state)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    Logger().info("Starting asset integrity verification...");

    // 获取所有资产项
    Types::ListParams list_params;
    list_params.per_page = 1000;

    auto asset_list_result = Repository::list_asset(app_state, list_params);
    if (!asset_list_result) {
      return std::unexpected("Failed to get asset list: " + asset_list_result.error());
    }

    auto asset_list = asset_list_result.value();

    Types::AssetScanResult result;
    result.total_files = asset_list.total_count;
    result.new_items = 0;
    result.updated_items = 0;
    result.deleted_items = 0;

    // 验证每个资产项的文件是否存在
    for (const auto& item : asset_list.items) {
      std::filesystem::path file_path(item.filepath);
      if (!std::filesystem::exists(file_path)) {
        result.errors.push_back(std::format("File not found: {}", item.filepath));
      }

      // 验证缩略图是否存在（对于照片类型）
      if (item.type == "photo" && item.thumbnail_path.has_value()) {
        auto thumbnail_path_result = Thumbnail::get_thumbnail_path(app_state, item);
        if (thumbnail_path_result) {
          auto thumbnail_path = thumbnail_path_result.value();
          if (!std::filesystem::exists(thumbnail_path)) {
            result.errors.push_back(
                std::format("Thumbnail not found: {}", thumbnail_path.string()));
          }
        }
      }
    }

    Logger().info("Asset integrity verification completed. Errors found: {}", result.errors.size());

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in verify_asset_integrity: " + std::string(e.what()));
  }
}

auto rebuild_asset_thumbnail_index(Core::State::AppState& app_state)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    Logger().info("Starting thumbnail index rebuild...");

    // 清理孤立的缩略图
    auto cleanup_result = Thumbnail::cleanup_orphaned_thumbnails(app_state);
    if (!cleanup_result) {
      Logger().warn("Failed to cleanup orphaned thumbnails: {}", cleanup_result.error());
    }

    // 重新生成所有缩略图
    auto generate_result = generate_missing_asset_thumbnails(app_state, 400, 400);
    if (!generate_result) {
      return std::unexpected("Failed to regenerate thumbnails: " + generate_result.error());
    }

    auto result = generate_result.value();
    result.deleted_items = cleanup_result.value_or(0);

    Logger().info("Thumbnail index rebuild completed");

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in rebuild_thumbnail_index: " + std::string(e.what()));
  }
}

// ============= 配置管理 =============

auto get_default_asset_scan_options() -> Types::AssetScanOptions {
  Types::AssetScanOptions options;
  options.recursive = true;
  options.generate_thumbnails = true;
  options.thumbnail_max_width = 400;
  options.thumbnail_max_height = 400;
  options.supported_extensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"};
  return options;
}

auto validate_asset_scan_params(const Types::ScanParams& params)
    -> std::expected<void, std::string> {
  if (params.directories.empty()) {
    return std::unexpected("No directories specified for scanning");
  }

  // 验证每个目录是否存在
  for (const auto& dir_str : params.directories) {
    std::filesystem::path dir_path(dir_str);
    if (!std::filesystem::exists(dir_path)) {
      return std::unexpected("Directory does not exist: " + dir_str);
    }
    if (!std::filesystem::is_directory(dir_path)) {
      return std::unexpected("Path is not a directory: " + dir_str);
    }
  }

  // 验证缩略图尺寸参数
  if (params.thumbnail_max_width.has_value() && params.thumbnail_max_width.value() < 32) {
    return std::unexpected("Thumbnail width too small (minimum 32px)");
  }
  if (params.thumbnail_max_width.has_value() && params.thumbnail_max_width.value() > 2048) {
    return std::unexpected("Thumbnail width too large (maximum 2048px)");
  }

  if (params.thumbnail_max_height.has_value() && params.thumbnail_max_height.value() < 32) {
    return std::unexpected("Thumbnail height too small (minimum 32px)");
  }
  if (params.thumbnail_max_height.has_value() && params.thumbnail_max_height.value() > 2048) {
    return std::unexpected("Thumbnail height too large (maximum 2048px)");
  }

  return {};
}

}  // namespace Features::Asset
