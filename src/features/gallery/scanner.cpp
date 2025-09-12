module;

#include <xxhash.h>

#include <format>
#include <latch>
#include <functional>
#include <iostream>

module Features.Gallery.Scanner;

import std;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Utils.Image;
import Utils.Logger;

namespace Features::Gallery::Scanner {

// ============= 工具函数 =============

// 计算文件哈希值
auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Cannot open file for hashing: " + file_path.string());
  }

  // 读取文件内容
  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  if (buffer.empty()) {
    return std::unexpected("File is empty: " + file_path.string());
  }

  // 计算XXH3哈希
  auto hash = XXH3_64bits(buffer.data(), buffer.size());

  return std::format("{:016x}", hash);
}

// ============= 文件扫描 =============

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

// ============= 资产信息提取 =============

auto extract_asset_info(Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& file_path)
    -> std::expected<Types::Info, std::string> {
  try {
    // 获取文件大小
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
      return std::unexpected("Failed to get file size: " + ec.message());
    }

    // 检测资产类型
    auto asset_type = detect_asset_type(file_path);

    Types::Info info;
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

// ============= 多线程扫描 =============

// 并行发现文件
auto discover_files_parallel(Core::State::AppState& app_state,
                             const std::vector<std::filesystem::path>& directories,
                             const Types::ScanOptions& options)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string> {
  if (directories.empty()) {
    return std::vector<Types::FileSystemInfo>{};
  }

  // 使用latch等待所有目录扫描完成
  std::latch completion_latch(directories.size());

  // 线程安全的结果收集
  std::vector<std::vector<Types::FileSystemInfo>> results(directories.size());

  for (size_t i = 0; i < directories.size(); ++i) {
    const auto& directory = directories[i];

    bool submitted = Core::WorkerPool::submit_task(*app_state.worker_pool, [&results, i,
                                                                            &completion_latch,
                                                                            directory, &options]() {
      auto files_result = find_files(directory, options.supported_extensions, options.recursive);
      if (!files_result) {
        Logger().error("Failed to scan directory {}: {}", directory.string(), files_result.error());
        completion_latch.count_down();
        return;
      }

      auto found_files = std::move(*files_result);
      results[i].reserve(found_files.size());

      for (const auto& file_path : found_files) {
        if (!is_file_accessible(file_path)) {
          continue;
        }

        // 使用 error_code 避免异常
        std::error_code ec;
        auto file_size = std::filesystem::file_size(file_path, ec);
        if (ec) continue;

        auto last_write_time = std::filesystem::last_write_time(file_path, ec);
        if (ec) continue;
        Types::FileSystemInfo info;
        info.filepath = file_path;
        info.file_size = static_cast<int64_t>(file_size);
        info.last_write_time = last_write_time;
        info.last_modified_str =
            std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());

        results[i].push_back(std::move(info));
      }

      completion_latch.count_down();
    });

    if (!submitted) {
      return std::unexpected("Failed to submit directory scan task to worker pool");
    }
  }

  // 优雅地等待所有任务完成，无CPU浪费
  completion_latch.wait();

  // 合并结果
  std::vector<Types::FileSystemInfo> all_files;
  size_t total_size = 0;
  for (const auto& result : results) {
    total_size += result.size();
  }
  all_files.reserve(total_size);

  for (auto& result : results) {
    all_files.insert(all_files.end(), std::make_move_iterator(result.begin()),
                     std::make_move_iterator(result.end()));
  }

  return all_files;
}

// 分析文件变化
auto analyze_file_changes(const std::vector<Types::FileSystemInfo>& discovered_files,
                          const Types::Cache& asset_cache)
    -> std::vector<Types::FileAnalysisResult> {
  std::vector<Types::FileAnalysisResult> results;
  results.reserve(discovered_files.size());

  for (const auto& file_info : discovered_files) {
    Types::FileAnalysisResult analysis;
    analysis.file_info = file_info;

    auto it = asset_cache.find(file_info.filepath.string());
    if (it == asset_cache.end()) {
      // 新文件
      analysis.status = Types::FileStatus::NEW;
    } else {
      // 检查是否需要更新
      const auto& cached_metadata = it->second;
      analysis.existing_metadata = cached_metadata;

      // 先用快速条件筛选出需要进一步校验的文件
      if (cached_metadata.file_size != file_info.file_size) {
        analysis.status = Types::FileStatus::NEEDS_HASH_CHECK;
      } else {
        analysis.status = Types::FileStatus::UNCHANGED;
      }
    }

    results.push_back(std::move(analysis));
  }

  return results;
}

// 计算目标文件哈希
auto calculate_hash_for_targets(Core::State::AppState& app_state,
                                std::vector<Types::FileAnalysisResult>& analysis_results)
    -> std::expected<void, std::string> {
  // 收集需要计算哈希的索引：新文件和需要校验的文件
  std::vector<size_t> target_indices;
  target_indices.reserve(analysis_results.size());
  for (size_t i = 0; i < analysis_results.size(); ++i) {
    const auto& a = analysis_results[i];
    if (a.status == Types::FileStatus::NEW || a.status == Types::FileStatus::NEEDS_HASH_CHECK) {
      target_indices.push_back(i);
    }
  }

  if (target_indices.empty()) {
    return {};
  }

  // 使用小批量分配，而不是按线程数分配
  constexpr size_t HASH_BATCH_SIZE = 32;  // 每批处理32个文件
  size_t total_batches = (target_indices.size() + HASH_BATCH_SIZE - 1) / HASH_BATCH_SIZE;

  // 使用latch等待所有批次完成
  std::latch completion_latch(total_batches);

  // 线程安全的结果收集 - 使用mutex保护的向量
  std::vector<std::pair<size_t, std::string>> all_hashes;
  std::mutex results_mutex;

  // 提交所有批次任务
  for (size_t batch_idx = 0; batch_idx < total_batches; ++batch_idx) {
    size_t start = batch_idx * HASH_BATCH_SIZE;
    size_t end = std::min(start + HASH_BATCH_SIZE, target_indices.size());

    bool submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool, [&all_hashes, &results_mutex, &completion_latch, start, end,
                                 &target_indices, &analysis_results]() {
          // 本批次的哈希结果
          std::vector<std::pair<size_t, std::string>> batch_hashes;
          batch_hashes.reserve(end - start);

          for (size_t t = start; t < end; ++t) {
            size_t idx = target_indices[t];
            const auto& file_path = analysis_results[idx].file_info.filepath;

            auto hash_result = calculate_file_hash(file_path);
            if (hash_result) {
              batch_hashes.emplace_back(idx, *hash_result);
            } else {
              Logger().warn("Failed to calculate hash for {}: {}", file_path.string(),
                            hash_result.error());
            }
          }

          // 线程安全地合并结果
          {
            std::lock_guard<std::mutex> lock(results_mutex);
            all_hashes.insert(all_hashes.end(), std::make_move_iterator(batch_hashes.begin()),
                              std::make_move_iterator(batch_hashes.end()));
          }

          completion_latch.count_down();
        });

    if (!submitted) {
      return std::unexpected("Failed to submit hash calculation task to worker pool");
    }
  }

  // 优雅地等待所有批次完成
  completion_latch.wait();

  // 根据哈希结果更新状态与信息
  for (const auto& [idx, hash] : all_hashes) {
    auto& analysis = analysis_results[idx];
    analysis.file_info.file_hash = hash;

    if (analysis.status == Types::FileStatus::NEEDS_HASH_CHECK) {
      if (analysis.existing_metadata && analysis.existing_metadata->file_hash &&
          analysis.existing_metadata->file_hash.value() == hash) {
        analysis.status = Types::FileStatus::UNCHANGED;
      } else {
        analysis.status = Types::FileStatus::MODIFIED;
      }
    }
    // NEW 保持 NEW 状态，后续处理用到 file_hash
  }

  return {};
}

// 并行处理文件
auto process_files_in_parallel(Core::State::AppState& app_state,
                               const std::vector<Types::FileAnalysisResult>& files_to_process,
                               const Types::ScanOptions& options)
    -> std::expected<Types::ProcessingBatchResult, std::string> {
  if (files_to_process.empty()) {
    return Types::ProcessingBatchResult{};
  }

  // 使用小批量分配，每批处理较少的文件以保证负载均衡
  constexpr size_t PROCESS_BATCH_SIZE = 16;  // 每批处理16个文件，可根据实际情况调整
  size_t total_batches = (files_to_process.size() + PROCESS_BATCH_SIZE - 1) / PROCESS_BATCH_SIZE;

  // 使用latch等待所有批次完成
  std::latch completion_latch(total_batches);

  // 线程安全的结果收集
  Types::ProcessingBatchResult final_result;
  std::mutex results_mutex;

  // 提交所有批次任务
  for (size_t batch_idx = 0; batch_idx < total_batches; ++batch_idx) {
    size_t start = batch_idx * PROCESS_BATCH_SIZE;
    size_t end = std::min(start + PROCESS_BATCH_SIZE, files_to_process.size());

    bool submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool, [&final_result, &results_mutex, &completion_latch, &app_state,
                                 &files_to_process, start, end, &options]() {
          // 每个工作批次需要自己的WIC工厂
          auto thread_wic_factory = Utils::Image::create_factory();
          if (!thread_wic_factory) {
            {
              std::lock_guard<std::mutex> lock(results_mutex);
              final_result.errors.push_back("Failed to create WIC factory in worker thread");
            }
            completion_latch.count_down();
            return;
          }

          // 本批次的结果
          Types::ProcessingBatchResult batch_result;

          for (size_t idx = start; idx < end; ++idx) {
            const auto& analysis = files_to_process[idx];

            auto asset_result =
                process_single_file_optimized(app_state, *thread_wic_factory, analysis, options);
            if (asset_result) {
              if (analysis.status == Types::FileStatus::NEW) {
                batch_result.new_assets.push_back(std::move(*asset_result));
              } else if (analysis.status == Types::FileStatus::MODIFIED) {
                batch_result.updated_assets.push_back(std::move(*asset_result));
              }
            } else {
              batch_result.errors.push_back(std::format(
                  "{}: {}", analysis.file_info.filepath.string(), asset_result.error()));
            }
          }

          // 线程安全地合并批次结果到最终结果
          {
            std::lock_guard<std::mutex> lock(results_mutex);

            final_result.new_assets.insert(final_result.new_assets.end(),
                                           std::make_move_iterator(batch_result.new_assets.begin()),
                                           std::make_move_iterator(batch_result.new_assets.end()));

            final_result.updated_assets.insert(
                final_result.updated_assets.end(),
                std::make_move_iterator(batch_result.updated_assets.begin()),
                std::make_move_iterator(batch_result.updated_assets.end()));

            final_result.errors.insert(final_result.errors.end(),
                                       std::make_move_iterator(batch_result.errors.begin()),
                                       std::make_move_iterator(batch_result.errors.end()));
          }

          completion_latch.count_down();
        });

    if (!submitted) {
      return std::unexpected("Failed to submit file processing task to worker pool");
    }
  }

  // 优雅地等待所有批次完成
  completion_latch.wait();

  return final_result;
}
// 优化版单文件处理
auto process_single_file_optimized(Core::State::AppState& app_state,
                                   Utils::Image::WICFactory& wic_factory,
                                   const Types::FileAnalysisResult& analysis,
                                   const Types::ScanOptions& options)
    -> std::expected<Types::Asset, std::string> {
  const auto& file_info = analysis.file_info;
  const auto& file_path = file_info.filepath;

  auto asset_type = detect_asset_type(file_path);

  Types::Asset asset;

  // 如果是更新，保留原有ID
  if (analysis.status == Types::FileStatus::MODIFIED && analysis.existing_metadata) {
    asset.id = analysis.existing_metadata->id;
  }

  asset.filename = file_path.filename().string();
  asset.filepath = file_path.string();
  asset.relative_path = file_path.filename().string();  // 简化
  asset.type = asset_type;
  asset.file_size = file_info.file_size;
  asset.file_hash = file_info.file_hash.value_or("");
  asset.created_at = file_info.last_modified_str;
  asset.updated_at = file_info.last_modified_str;

  if (asset_type == "photo") {
    auto image_info_result = Utils::Image::get_image_info(wic_factory.get(), file_path);
    if (image_info_result) {
      auto image_info = std::move(*image_info_result);
      asset.width = image_info.width;
      asset.height = image_info.height;
      asset.mime_type = std::move(image_info.mime_type);
    } else {
      Logger().warn("Could not extract image info from {}: {}", file_path.string(),
                    image_info_result.error());
      asset.width = 0;
      asset.height = 0;
      asset.mime_type = "application/octet-stream";
    }

    // 生成缩略图（如果需要）
    if (options.generate_thumbnails) {
      auto thumbnail_result = Asset::Thumbnail::generate_thumbnail(
          app_state, wic_factory, file_path, file_info.file_hash.value_or(""),
          options.thumbnail_max_width, options.thumbnail_max_height);

      if (!thumbnail_result) {
        Logger().warn("Failed to generate thumbnail for {}: {}", file_path.string(),
                      thumbnail_result.error());
      }
    }
  } else {
    asset.width = 0;
    asset.height = 0;
    asset.mime_type = "application/octet-stream";
  }

  return asset;
}

// 主扫描函数
auto scan_asset_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string> {
  auto start_time = std::chrono::steady_clock::now();

  Logger().info("Starting optimized multi-threaded asset scan with {} directories",
                options.directories.size());

  // 加载资产缓存
  auto asset_cache_result = Asset::Repository::load_asset_cache(app_state);
  if (!asset_cache_result) {
    return std::unexpected("Failed to load asset cache: " + asset_cache_result.error());
  }
  auto asset_cache = std::move(*asset_cache_result);

  // 转换目录路径
  std::vector<std::filesystem::path> directories;
  for (const auto& dir_str : options.directories) {
    directories.emplace_back(dir_str);
  }

  // 并行发现文件
  auto discovered_files_result = discover_files_parallel(app_state, directories, options);
  if (!discovered_files_result) {
    return std::unexpected("File discovery failed: " + discovered_files_result.error());
  }
  auto discovered_files = discovered_files_result.value();

  Logger().info("Discovered {} files across {} directories", discovered_files.size(),
                directories.size());

  // 分析文件变化
  auto analysis_results = analyze_file_changes(discovered_files, asset_cache);

  // 计算文件哈希
  if (auto hash_phase = calculate_hash_for_targets(app_state, analysis_results); !hash_phase) {
    return std::unexpected("Hash calculation failed: " + hash_phase.error());
  }

  // 筛选需要处理的文件
  std::vector<Types::FileAnalysisResult> files_to_process;
  std::ranges::copy_if(analysis_results, std::back_inserter(files_to_process),
                       [](const Types::FileAnalysisResult& result) {
                         return result.status == Types::FileStatus::NEW ||
                                result.status == Types::FileStatus::MODIFIED;
                       });

  Logger().info("Found {} files that need processing", files_to_process.size());

  if (files_to_process.empty()) {
    // 无需处理文件
    Types::ScanResult result;
    result.total_files = static_cast<int>(discovered_files.size());
    result.new_items = 0;
    result.updated_items = 0;
    result.deleted_items = 0;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.scan_duration = std::format("{}ms", duration.count());

    Logger().info("Asset scan completed - no changes detected");
    return result;
  }

  // 并行处理文件
  auto processing_result = process_files_in_parallel(app_state, files_to_process, options);
  if (!processing_result) {
    return std::unexpected("File processing failed: " + processing_result.error());
  }

  auto batch_result = processing_result.value();

  // 批量数据库操作
  bool all_db_success = true;

  if (!batch_result.new_assets.empty()) {
    auto create_result = Asset::Repository::batch_create_asset(app_state, batch_result.new_assets);
    if (create_result) {
      Logger().info("Successfully created {} new asset items", batch_result.new_assets.size());
    } else {
      Logger().error("Failed to batch create asset items: {}", create_result.error());
      all_db_success = false;
    }
  }

  if (!batch_result.updated_assets.empty()) {
    auto update_result = Asset::Repository::batch_update_asset(app_state, batch_result.updated_assets);
    if (update_result) {
      Logger().info("Successfully updated {} asset items", batch_result.updated_assets.size());
    } else {
      Logger().error("Failed to batch update asset items: {}", update_result.error());
      all_db_success = false;
    }
  }

  // 构建扫描结果
  Types::ScanResult result;
  result.total_files = static_cast<int>(discovered_files.size());
  result.new_items = static_cast<int>(batch_result.new_assets.size());
  result.updated_items = static_cast<int>(batch_result.updated_assets.size());
  result.deleted_items = 0;
  result.errors = std::move(batch_result.errors);

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.scan_duration = std::format("{}ms", duration.count());

  if (!all_db_success) {
    result.errors.push_back("Some database operations failed");
  }

  Logger().info(
      "Multi-threaded asset scan completed. Total: {}, New: {}, Updated: {}, Errors: {}, Duration: "
      "{}",
      result.total_files, result.new_items, result.updated_items, result.errors.size(),
      result.scan_duration);

  return result;
}

}  // namespace Features::Gallery::Scanner
