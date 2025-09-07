module;

#include <format>

module Features.Asset.Scanner;

import std;
import Core.State;
import Features.Asset.Types;
import Features.Asset.Repository;
import Utils.Image;
import Utils.Logger;

namespace Features::Asset::Scanner {

// ============= 文件扫描功能 =============

auto find_files(const std::filesystem::path& directory,
                      const std::vector<std::string>& supported_extensions, bool recursive)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
  if (!std::filesystem::exists(directory)) {
    return std::unexpected("Directory does not exist: " + directory.string());
  }

  if (!std::filesystem::is_directory(directory)) {
    return std::unexpected("Path is not a directory: " + directory.string());
  }

  try {
    std::vector<std::filesystem::path> found_files;

    if (recursive) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        try {
          if (entry.is_regular_file()) {
            if (is_supported_file(entry.path(), supported_extensions)) {
              found_files.push_back(entry.path());
            }
          }
        } catch (const std::filesystem::filesystem_error& e) {
          // 跳过无法访问的文件，但不中断整个扫描
          Logger().warn("Skipping file due to access error: {}", e.what());
          continue;
        }
      }
    } else {
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        try {
          if (entry.is_regular_file()) {
            if (is_supported_file(entry.path(), supported_extensions)) {
              found_files.push_back(entry.path());
            }
          }
        } catch (const std::filesystem::filesystem_error& e) {
          // 跳过无法访问的文件，但不中断整个扫描
          Logger().warn("Skipping file due to access error: {}", e.what());
          continue;
        }
      }
    }

    return found_files;

  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Filesystem error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Exception in find_media_files: " + std::string(e.what()));
  }
}

auto is_supported_file(const std::filesystem::path& file_path,
                             const std::vector<std::string>& supported_extensions) -> bool {
  if (!file_path.has_extension()) {
    return false;
  }

  std::string extension = file_path.extension().string();

  // 转换为小写进行比较
  std::ranges::transform(extension, extension.begin(),
                         [](unsigned char c) { return static_cast<char>(::tolower(c)); });

  return std::ranges::find(supported_extensions, extension) != supported_extensions.end();
}

auto is_file_accessible(const std::filesystem::path& file_path) -> bool {
  try {
    return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
  } catch (const std::filesystem::filesystem_error&) {
    return false;
  }
}

auto detect_asset_type(const std::filesystem::path& file_path) -> std::string {
  if (!file_path.has_extension()) {
    return "unknown";
  }

  std::string extension = file_path.extension().string();
  std::ranges::transform(extension, extension.begin(),
                         [](unsigned char c) { return static_cast<char>(::tolower(c)); });

  // 图片格式
  if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp" ||
      extension == ".webp" || extension == ".tiff" || extension == ".tif") {
    return "photo";
  }

  // 视频格式（预留）
  if (extension == ".mp4" || extension == ".avi" || extension == ".mov" || extension == ".mkv" ||
      extension == ".wmv" || extension == ".webm") {
    return "video";
  }

  return "unknown";
}

auto calculate_asset_relative_path(const std::filesystem::path& file_path,
                             const std::filesystem::path& base_directory) -> std::string {
  try {
    auto relative = std::filesystem::relative(file_path, base_directory);
    return relative.string();
  } catch (const std::filesystem::filesystem_error&) {
    // 如果无法计算相对路径，返回文件名
    return file_path.filename().string();
  }
}

// ============= 资产信息提取功能 =============

auto extract_asset_info(Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& file_path)
    -> std::expected<Types::AssetInfo, std::string> {
  try {
    // 获取文件大小
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
      return std::unexpected("Failed to get file size: " + ec.message());
    }

    // 检测资产类型
    auto asset_type = detect_asset_type(file_path);

    Types::AssetInfo info;
    info.file_size = static_cast<int64_t>(file_size);
    info.detected_type = asset_type;

    // 如果是图片，使用 Utils::Image 获取详细信息
    if (asset_type == "photo") {
      auto image_info_result = Utils::Image::get_image_info(wic_factory.get(), file_path);
      if (image_info_result) {
        auto image_info = image_info_result.value();
        info.width = image_info.width;
        info.height = image_info.height;
        info.mime_type = image_info.mime_type;
      } else {
        // 无法获取图像信息，但不失败，设置默认值
        Logger().warn("Could not extract image info from {}: {}", file_path.string(),
                      image_info_result.error());
        info.width = 0;
        info.height = 0;
        info.mime_type = "application/octet-stream";
      }
    } else {
      // 非图片类型，设置默认值
      info.width = 0;
      info.height = 0;
      info.mime_type = "application/octet-stream";
    }

    return info;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in extract_media_info: " + std::string(e.what()));
  }
}

// ============= 批量处理功能 =============

auto process_single_file(Core::State::AppState& app_state,
                               Utils::Image::WICFactory& wic_factory,
                               const std::filesystem::path& file_path,
                               const std::filesystem::path& base_directory, bool generate_thumbnail)
    -> std::expected<Types::Asset, std::string> {
  try {
    // 提取资产信息
    auto media_info_result = extract_asset_info(wic_factory, file_path);
    if (!media_info_result) {
      return std::unexpected("Failed to extract asset info: " + media_info_result.error());
    }
    auto asset_info = media_info_result.value();

    // 创建 Asset
    Types::Asset item;
    item.filename = file_path.filename().string();
    item.filepath = file_path.string();
    item.relative_path = calculate_asset_relative_path(file_path, base_directory);
    item.type = asset_info.detected_type;
    item.width = asset_info.width;
    item.height = asset_info.height;
    item.file_size = asset_info.file_size;
    item.mime_type = asset_info.mime_type;

    // 设置时间戳
    auto now = std::chrono::system_clock::now();
    item.created_at = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    item.updated_at = item.created_at;

    // 如果需要生成缩略图，将在 Thumbnail 模块中处理
    // 这里只是预设路径
    if (generate_thumbnail && item.type == "photo") {
      std::string thumbnail_filename =
          std::format("{}.webp", std::filesystem::path(item.filename).stem().string());
      item.thumbnail_path = thumbnail_filename;
    }

    return item;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in process_single_media_file: " + std::string(e.what()));
  }
}

auto process_files(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                         const std::vector<std::filesystem::path>& file_paths,
                         const std::filesystem::path& base_directory, bool generate_thumbnails)
    -> std::expected<std::vector<Types::Asset>, std::string> {
  try {
    std::vector<Types::Asset> items;
    items.reserve(file_paths.size());

    for (const auto& file_path : file_paths) {
      auto item_result = process_single_file(app_state, wic_factory, file_path,
                                                   base_directory, generate_thumbnails);
      if (item_result) {
        items.push_back(std::move(item_result.value()));
      } else {
        Logger().warn("Failed to process file {}: {}", file_path.string(), item_result.error());
        // 继续处理其他文件，不因单个文件失败而中断
      }
    }

    return items;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in process_media_files: " + std::string(e.what()));
  }
}

// ============= 增量扫描功能 =============

auto is_file_already_indexed(Core::State::AppState& app_state,
                             const std::filesystem::path& file_path)
    -> std::expected<bool, std::string> {
  return Repository::filepath_exists(app_state, file_path.string());
}

auto needs_update(Core::State::AppState& app_state, const std::filesystem::path& file_path)
    -> std::expected<bool, std::string> {
  try {
    // 获取现有记录
    auto existing_item_result = Repository::get_asset_by_filepath(app_state, file_path.string());
    if (!existing_item_result) {
      return std::unexpected("Failed to check existing item: " + existing_item_result.error());
    }

    if (!existing_item_result->has_value()) {
      // 文件不存在于数据库中，需要添加
      return true;
    }

    // 获取文件的最后修改时间
    std::error_code ec;
    auto last_write_time = std::filesystem::last_write_time(file_path, ec);
    if (ec) {
      Logger().warn("Could not get last write time for {}: {}", file_path.string(), ec.message());
      // 如果无法获取修改时间，假设需要更新
      return true;
    }

    // 这里可以比较文件修改时间和数据库中的 updated_at
    // 简化版本：如果文件存在但大小不同，则需要更新
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
      return true;  // 无法获取文件大小，假设需要更新
    }

    auto existing_item = existing_item_result->value();
    if (existing_item.file_size.has_value() &&
        existing_item.file_size.value() != static_cast<int64_t>(file_size)) {
      return true;
    }

    return false;  // 不需要更新

  } catch (const std::exception& e) {
    return std::unexpected("Exception in needs_update: " + std::string(e.what()));
  }
}

// ============= 主扫描功能 =============

auto scan_asset_directory(Core::State::AppState& app_state, const Types::AssetScanOptions& options)
    -> std::expected<Types::AssetScanResult, std::string> {
  try {
    AssetScanStatistics stats;
    stats.start_time = std::chrono::steady_clock::now();

    Logger().info("Starting asset scan with {} directories", options.directories.size());

    // 创建 WIC 工厂
    auto wic_factory_result = Utils::Image::create_factory();
    if (!wic_factory_result) {
      return std::unexpected("Failed to create WIC factory: " + wic_factory_result.error());
    }
    auto wic_factory = wic_factory_result.value();

    std::vector<Types::Asset> all_new_items;
    std::vector<Types::Asset> all_updated_items;

    // 处理每个目录
    for (const auto& dir_str : options.directories) {
      std::filesystem::path directory(dir_str);

      Logger().info("Scanning directory: {}", directory.string());

      // 查找资产文件
      auto files_result =
          find_files(directory, options.supported_extensions, options.recursive);
      if (!files_result) {
        AssetScanError error;
        error.file_path = directory;
        error.error_message = files_result.error();
        error.error_type = "directory_scan_error";
        stats.errors.push_back(error);
        stats.errors_count++;
        Logger().error("Failed to scan directory {}: {}", directory.string(), files_result.error());
        continue;  // 继续处理下一个目录
      }

      auto found_files = files_result.value();
      stats.total_files_found += static_cast<int>(found_files.size());

      Logger().info("Found {} asset files in {}", found_files.size(), directory.string());

      // 处理找到的文件
      for (const auto& file_path : found_files) {
        try {
          // 检查文件是否可访问
          if (!is_file_accessible(file_path)) {
            AssetScanError error;
            error.file_path = file_path;
            error.error_message = "File not accessible";
            error.error_type = "access_denied";
            stats.errors.push_back(error);
            stats.errors_count++;
            continue;
          }

          // 检查是否需要处理此文件
          auto needs_processing_result = needs_update(app_state, file_path);
          if (!needs_processing_result) {
            Logger().warn("Could not check if file needs update: {}",
                          needs_processing_result.error());
            // 继续处理，假设需要更新
          } else if (!needs_processing_result.value()) {
            stats.files_skipped++;
            continue;
          }

          // 处理文件
          auto item_result = process_single_file(app_state, wic_factory, file_path, directory,
                                                       options.generate_thumbnails);
          if (!item_result) {
            AssetScanError error;
            error.file_path = file_path;
            error.error_message = item_result.error();
            error.error_type = "processing_error";
            stats.errors.push_back(error);
            stats.errors_count++;
            continue;
          }

          auto item = item_result.value();

          // 检查是新文件还是更新文件
          auto exists_result = is_file_already_indexed(app_state, file_path);
          if (exists_result && exists_result.value()) {
            all_updated_items.push_back(item);
            stats.files_updated++;
          } else {
            all_new_items.push_back(item);
            stats.new_files_added++;
          }

          stats.files_processed++;

        } catch (const std::exception& e) {
          AssetScanError error;
          error.file_path = file_path;
          error.error_message = std::string("Exception: ") + e.what();
          error.error_type = "processing_error";
          stats.errors.push_back(error);
          stats.errors_count++;
        }
      }
    }

    // 批量保存到数据库
    bool database_success = true;

    if (!all_new_items.empty()) {
      auto batch_create_result = Repository::batch_create_asset(app_state, all_new_items);
      if (!batch_create_result) {
        Logger().error("Failed to batch create asset items: {}", batch_create_result.error());
        database_success = false;
        // 不立即返回，先尝试更新操作
      } else {
        Logger().info("Successfully created {} new asset items", all_new_items.size());
      }
    }

    if (!all_updated_items.empty()) {
      auto batch_update_result = Repository::batch_update_asset(app_state, all_updated_items);
      if (!batch_update_result) {
        Logger().error("Failed to batch update asset items: {}", batch_update_result.error());
        database_success = false;
      } else {
        Logger().info("Successfully updated {} asset items", all_updated_items.size());
      }
    }

    // 如果数据库操作失败，但文件扫描成功，返回部分成功的结果
    if (!database_success) {
      Logger().warn("Some database operations failed, but file scanning completed successfully");
    }

    stats.end_time = std::chrono::steady_clock::now();

    Logger().info("Asset scan completed. Processed: {}, New: {}, Updated: {}, Errors: {}",
                  stats.files_processed, stats.new_files_added, stats.files_updated,
                  stats.errors_count);

    return asset_statistics_to_result(stats);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in scan_directory: " + std::string(e.what()));
  }
}

// ============= 统计功能 =============

auto AssetScanStatistics::get_duration_string() const -> std::string {
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  return std::format("{}ms", duration.count());
}

auto asset_statistics_to_result(const AssetScanStatistics& stats) -> Types::AssetScanResult {
  Types::AssetScanResult result;
  result.total_files = stats.total_files_found;
  result.new_items = stats.new_files_added;
  result.updated_items = stats.files_updated;
  result.deleted_items = 0;  // 扫描过程中不会删除项目
  result.scan_duration = stats.get_duration_string();

  // 转换错误信息
  for (const auto& error : stats.errors) {
    result.errors.push_back(std::format("{}: {} ({})", error.file_path.string(),
                                        error.error_message, error.error_type));
  }

  return result;
}

}  // namespace Features::Asset::Scanner
