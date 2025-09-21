module;

module Features.Gallery.Scanner;

import std;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Processor;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Ignore.Processor;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.Time;
import Vendor.BuildConfig;
import Vendor.XXHash;

namespace Features::Gallery::Scanner {

auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  if (Vendor::BuildConfig::is_debug_build()) {
    // Debug模式：直接hash文件路径，避免XXH3性能问题
    auto path_str = file_path.string();
    auto hash = std::hash<std::string>{}(path_str);
    return std::format("{:016x}", hash);
  }

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
  auto hash = Vendor::XXHash::HashCharVectorToHex(buffer);

  return hash;
}

auto is_supported_file(const std::filesystem::path& file_path,
                       const std::vector<std::string>& supported_extensions) -> bool {
  if (!file_path.has_extension()) {
    return false;
  }

  std::string extension = file_path.extension().string();

  // 转换为小写进行比较
  std::ranges::transform(extension, extension.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  return std::ranges::find(supported_extensions, extension) != supported_extensions.end();
}

auto scan_paths(Core::State::AppState& app_state, const std::filesystem::path& directory,
                const Types::ScanOptions& options, std::optional<std::int64_t> folder_id)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
  if (!std::filesystem::exists(directory)) {
    return std::unexpected("Directory does not exist: " + directory.string());
  }

  if (!std::filesystem::is_directory(directory)) {
    return std::unexpected("Path is not a directory: " + directory.string());
  }

  try {
    // 加载并合并忽略规则
    std::vector<Types::IgnoreRule> combined_rules;

    // 先加载全局规则
    auto global_rules_result = Ignore::Repository::get_global_rules(app_state);
    if (global_rules_result) {
      combined_rules = std::move(global_rules_result.value());
    }

    // 然后追加文件夹特定规则
    if (folder_id.has_value()) {
      auto folder_rules_result = Ignore::Repository::get_rules_by_folder_id(app_state, *folder_id);
      if (folder_rules_result) {
        auto& folder_rules = folder_rules_result.value();
        combined_rules.insert(combined_rules.end(), folder_rules.begin(), folder_rules.end());
      }
    }

    // 使用递归目录迭代器和ranges管道进行函数式过滤和转换
    auto found_files =
        std::ranges::subrange(std::filesystem::recursive_directory_iterator(directory),
                              std::filesystem::recursive_directory_iterator{}) |
        std::views::filter([](const std::filesystem::directory_entry& entry) {
          try {
            return entry.is_regular_file();
          } catch (const std::filesystem::filesystem_error& e) {
            Logger().warn("Skipping file due to access error: {}", e.what());
            return false;
          }
        }) |
        std::views::filter([&options](const std::filesystem::directory_entry& entry) {
          return is_supported_file(entry.path(), options.supported_extensions);
        }) |
        std::views::filter(
            [&directory, &combined_rules](const std::filesystem::directory_entry& entry) {
              bool should_ignore =
                  Ignore::Processor::apply_ignore_rules(entry.path(), directory, combined_rules);

              if (should_ignore) {
                Logger().debug("File ignored by rules: {}", entry.path().string());
              }

              return !should_ignore;
            }) |
        std::views::transform(
            [](const std::filesystem::directory_entry& entry) { return entry.path(); }) |
        std::ranges::to<std::vector>();

    return found_files;

  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Filesystem error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Exception in scan_paths: " + std::string(e.what()));
  }
}

auto detect_asset_type(const std::filesystem::path& file_path) -> std::string {
  if (!file_path.has_extension()) {
    return "unknown";
  }

  std::string extension = file_path.extension().string();
  std::ranges::transform(extension, extension.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

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

// 扫描目录并获取文件信息
auto scan_file_info(Core::State::AppState& app_state, const std::filesystem::path& directory,
                    const Types::ScanOptions& options, std::optional<std::int64_t> folder_id)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string> {
  auto files_result = scan_paths(app_state, directory, options, folder_id);
  if (!files_result) {
    return std::unexpected("Failed to scan directory " + directory.string() + ": " +
                           files_result.error());
  }

  auto found_files = std::move(files_result.value());
  std::vector<Types::FileSystemInfo> result;
  result.reserve(found_files.size());

  for (const auto& file_path : found_files) {
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) continue;

    auto last_write_time = std::filesystem::last_write_time(file_path, ec);
    if (ec) continue;

    auto creation_time_result = Utils::Time::get_file_creation_time_millis(file_path);
    if (!creation_time_result) {
      Logger().debug("Could not get creation time for {}: {}", file_path.string(),
                     creation_time_result.error());
      continue;
    }

    Types::FileSystemInfo info{
        .filepath = file_path,
        .size = static_cast<int64_t>(file_size),
        .file_modified_millis = Utils::Time::file_time_to_millis(last_write_time),
        .file_created_millis = creation_time_result.value(),
        .hash = ""};

    result.push_back(std::move(info));
  }

  Logger().info("Scanned {} files with ignore rules applied", result.size());
  return result;
}

// 分析文件变化
auto analyze_file_changes(const std::vector<Types::FileSystemInfo>& file_infos,
                          const std::unordered_map<std::string, Types::Metadata>& asset_cache)
    -> std::vector<Types::FileAnalysisResult> {
  std::vector<Types::FileAnalysisResult> results;
  results.reserve(file_infos.size());

  for (const auto& file_info : file_infos) {
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

      // 检查文件大小和修改时间，任一不同都需要重新计算哈希
      if (cached_metadata.size != file_info.size ||
          cached_metadata.file_modified_at != file_info.file_modified_millis) {
        analysis.status = Types::FileStatus::NEEDS_HASH_CHECK;
      } else {
        // 大小和修改时间都相同，很可能未发生变化
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
  // 1. 使用ranges找到需要处理的文件，保持原始索引
  auto targets_with_index = analysis_results | std::views::enumerate |
                            std::views::filter([](const auto& pair) {
                              const auto& [idx, analysis] = pair;
                              return analysis.status == Types::FileStatus::NEW ||
                                     analysis.status == Types::FileStatus::NEEDS_HASH_CHECK;
                            }) |
                            std::ranges::to<std::vector>();

  if (targets_with_index.empty()) {
    return {};
  }

  constexpr size_t HASH_BATCH_SIZE = 32;
  auto batches =
      targets_with_index | std::views::chunk(HASH_BATCH_SIZE) | std::ranges::to<std::vector>();

  std::latch completion_latch(batches.size());
  std::vector<std::pair<size_t, std::string>> all_hashes;
  std::mutex results_mutex;

  // 2. 并行处理批次
  for (const auto& batch : batches) {
    bool submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool,
        [&all_hashes, &results_mutex, &completion_latch, batch, &analysis_results]() {
          auto batch_hashes =
              batch |
              std::views::transform(
                  [](const auto& pair) -> std::optional<std::pair<size_t, std::string>> {
                    const auto& [idx, analysis] = pair;

                    auto hash_result = calculate_file_hash(analysis.file_info.filepath);
                    if (hash_result) {
                      return std::make_pair(idx, std::move(hash_result.value()));
                    } else {
                      Logger().warn("Failed to calculate hash for {}: {}",
                                    analysis.file_info.filepath.string(), hash_result.error());
                      return std::nullopt;
                    }
                  }) |
              std::views::filter([](const auto& opt) { return opt.has_value(); }) |
              std::views::transform([](auto&& opt) { return std::move(opt.value()); }) |
              std::ranges::to<std::vector>();

          // 合并结果
          if (!batch_hashes.empty()) {
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

  // 等待所有批次完成
  completion_latch.wait();

  // 3. 更新分析结果
  std::ranges::for_each(all_hashes, [&analysis_results](const auto& hash_pair) {
    const auto& [idx, hash] = hash_pair;
    auto& analysis = analysis_results[idx];
    analysis.file_info.hash = hash;

    // 根据哈希结果更新状态
    if (analysis.status == Types::FileStatus::NEEDS_HASH_CHECK) {
      const bool hash_unchanged = analysis.existing_metadata &&
                                  !analysis.existing_metadata->hash.empty() &&
                                  analysis.existing_metadata->hash == hash;

      analysis.status = hash_unchanged ? Types::FileStatus::UNCHANGED : Types::FileStatus::MODIFIED;
    }
  });

  return {};
}

// 处理单个文件
auto process_single_file(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                         const Types::FileAnalysisResult& analysis,
                         const Types::ScanOptions& options,
                         const std::unordered_map<std::string, std::int64_t>& folder_mapping)
    -> std::expected<Types::Asset, std::string> {
  const auto& file_info = analysis.file_info;
  const auto& file_path = file_info.filepath;

  auto asset_type = detect_asset_type(file_path);

  Types::Asset asset;

  // 如果是更新，保留原有ID
  if (analysis.status == Types::FileStatus::MODIFIED && analysis.existing_metadata) {
    asset.id = analysis.existing_metadata->id;
  }

  asset.name = file_path.filename().string();
  asset.filepath = file_path.string();
  asset.type = asset_type;
  asset.size = file_info.size;
  asset.hash = file_info.hash.empty() ? std::nullopt : std::optional<std::string>{file_info.hash};

  // 设置文件系统时间戳
  asset.file_created_at = file_info.file_created_millis;
  asset.file_modified_at = file_info.file_modified_millis;

  // 数据库记录管理时间戳由数据库自动设置

  // 根据文件路径查找对应的 folder_id
  if (!folder_mapping.empty()) {
    auto parent_path_result = Utils::Path::NormalizePath(file_path.parent_path());
    if (!parent_path_result) {
      return std::unexpected(std::format("Failed to normalize parent path for '{}': {}",
                                         file_path.string(), parent_path_result.error()));
    }
    auto parent_path = parent_path_result.value().string();
    if (auto it = folder_mapping.find(parent_path); it != folder_mapping.end()) {
      asset.folder_id = it->second;
    }
  }

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
          app_state, wic_factory, file_path, file_info.hash, options.thumbnail_max_width,
          options.thumbnail_max_height);

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

// 并行处理文件
auto process_files_in_parallel(Core::State::AppState& app_state,
                               const std::vector<Types::FileAnalysisResult>& files_to_process,
                               const Types::ScanOptions& options,
                               const std::unordered_map<std::string, std::int64_t>& folder_mapping)
    -> std::expected<Types::ProcessingBatchResult, std::string> {
  if (files_to_process.empty()) {
    return Types::ProcessingBatchResult{};
  }

  constexpr size_t PROCESS_BATCH_SIZE = 16;  // 每批处理16个文件，可根据实际情况调整
  size_t total_batches = (files_to_process.size() + PROCESS_BATCH_SIZE - 1) / PROCESS_BATCH_SIZE;

  std::latch completion_latch(total_batches);

  Types::ProcessingBatchResult final_result;
  std::mutex results_mutex;

  // 提交所有批次任务
  for (size_t batch_idx = 0; batch_idx < total_batches; ++batch_idx) {
    size_t start = batch_idx * PROCESS_BATCH_SIZE;
    size_t end = std::min(start + PROCESS_BATCH_SIZE, files_to_process.size());

    bool submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool, [&final_result, &results_mutex, &completion_latch, &app_state,
                                 &files_to_process, start, end, &options, &folder_mapping]() {
          auto thread_wic_factory_result = Utils::Image::get_thread_wic_factory();
          if (!thread_wic_factory_result) {
            {
              std::lock_guard<std::mutex> lock(results_mutex);
              final_result.errors.push_back("Failed to get thread WIC factory: " +
                                            thread_wic_factory_result.error());
            }
            completion_latch.count_down();
            return;
          }
          auto thread_wic_factory = std::move(thread_wic_factory_result.value());

          // 本批次的结果
          Types::ProcessingBatchResult batch_result;

          for (size_t idx = start; idx < end; ++idx) {
            const auto& analysis = files_to_process[idx];

            auto asset_result = process_single_file(app_state, thread_wic_factory, analysis,
                                                    options, folder_mapping);
            if (asset_result) {
              if (analysis.status == Types::FileStatus::NEW) {
                batch_result.new_assets.push_back(std::move(asset_result.value()));
              } else if (analysis.status == Types::FileStatus::MODIFIED) {
                batch_result.updated_assets.push_back(std::move(asset_result.value()));
              }
            } else {
              batch_result.errors.push_back(std::format(
                  "{}: {}", analysis.file_info.filepath.string(), asset_result.error()));
            }
          }

          // 合并批次结果到最终结果
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

  // 等待所有批次完成
  completion_latch.wait();

  return final_result;
}

// 主扫描函数
auto scan_asset_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string> {
  auto start_time = std::chrono::steady_clock::now();

  Logger().info("Starting folder-aware asset scan for directory '{}' with {} ignore rules",
                options.directory, options.ignore_rules.size());

  // 1. 确保文件夹记录存在
  auto folder_id_result =
      Folder::Repository::get_or_create_folder_for_path(app_state, options.directory);
  if (!folder_id_result) {
    return std::unexpected("Failed to create folder record: " + folder_id_result.error());
  }
  std::int64_t folder_id = folder_id_result.value();

  // 2. 持久化前端提供的ignore规则
  if (!options.ignore_rules.empty()) {
    auto persist_result =
        Ignore::Repository::batch_create_ignore_rules(app_state, folder_id, options.ignore_rules);
    if (!persist_result) {
      Logger().warn("Failed to persist ignore rules: {}", persist_result.error());
    } else {
      Logger().info("Persisted {} new ignore rules for folder_id {}", persist_result.value().size(),
                    folder_id);
    }
  }

  // 3. 加载资产缓存
  auto asset_cache_result = Asset::Repository::load_asset_cache(app_state);
  if (!asset_cache_result) {
    return std::unexpected("Failed to load asset cache: " + asset_cache_result.error());
  }
  auto asset_cache = std::move(asset_cache_result.value());

  std::filesystem::path directory(options.directory);

  // 4. 使用支持忽略规则的文件信息扫描（传递folder_id）
  auto file_info_result = scan_file_info(app_state, directory, options, folder_id);
  if (!file_info_result) {
    return std::unexpected("File info scanning failed: " + file_info_result.error());
  }
  auto file_infos = file_info_result.value();

  Logger().info("Scanned {} files in directory '{}' (after ignore rules)", file_infos.size(),
                options.directory);

  // 分析文件变化
  auto analysis_results = analyze_file_changes(file_infos, asset_cache);

  // 计算文件哈希
  if (auto hash_phase = calculate_hash_for_targets(app_state, analysis_results); !hash_phase) {
    return std::unexpected("Hash calculation failed: " + hash_phase.error());
  }

  Logger().info("Calculated hashes for {} files", analysis_results.size());

  // 5. 筛选需要处理的文件
  std::vector<Types::FileAnalysisResult> files_to_process;
  std::ranges::copy_if(analysis_results, std::back_inserter(files_to_process),
                       [](const Types::FileAnalysisResult& result) {
                         return result.status == Types::FileStatus::NEW ||
                                result.status == Types::FileStatus::MODIFIED;
                       });

  Logger().info("Found {} files that need processing", files_to_process.size());

  if (files_to_process.empty()) {
    // 无需处理文件
    Types::ScanResult result{.total_files = static_cast<int>(file_infos.size())};

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.scan_duration = std::format("{}ms", duration.count());

    Logger().info("Folder-aware asset scan completed - no changes detected");
    return result;
  }

  // 6. 提前准备文件夹映射，批量创建文件夹记录
  std::unordered_map<std::string, std::int64_t> path_to_folder_id;
  std::vector<std::filesystem::path> file_paths;
  file_paths.reserve(files_to_process.size());

  for (const auto& analysis : files_to_process) {
    file_paths.push_back(analysis.file_info.filepath);
  }

  auto folder_paths = Folder::Processor::extract_unique_folder_paths(file_paths);

  auto folder_mapping_result =
      Folder::Processor::batch_create_folders_for_paths(app_state, folder_paths);
  if (folder_mapping_result) {
    path_to_folder_id = std::move(folder_mapping_result.value());
    Logger().info("Pre-created {} folder records", path_to_folder_id.size());
  } else {
    Logger().warn("Failed to pre-create folders: {}", folder_mapping_result.error());
  }

  // 7. 并行处理文件
  auto processing_result =
      process_files_in_parallel(app_state, files_to_process, options, path_to_folder_id);
  if (!processing_result) {
    return std::unexpected("File processing failed: " + processing_result.error());
  }

  auto batch_result = processing_result.value();

  // 8. 批量更新资产记录
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
    auto update_result =
        Asset::Repository::batch_update_asset(app_state, batch_result.updated_assets);
    if (update_result) {
      Logger().info("Successfully updated {} asset items", batch_result.updated_assets.size());
    } else {
      Logger().error("Failed to batch update asset items: {}", update_result.error());
      all_db_success = false;
    }
  }

  // 9. 构建扫描结果
  Types::ScanResult result{.total_files = static_cast<int>(file_infos.size()),
                           .new_items = static_cast<int>(batch_result.new_assets.size()),
                           .updated_items = static_cast<int>(batch_result.updated_assets.size()),
                           .errors = std::move(batch_result.errors)};

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.scan_duration = std::format("{}ms", duration.count());

  if (!all_db_success) {
    result.errors.push_back("Some database operations failed");
  }

  Logger().info(
      "Folder-aware asset scan completed. Total: {}, New: {}, Updated: {}, Errors: {}, Duration: "
      "{}",
      result.total_files, result.new_items, result.updated_items, result.errors.size(),
      result.scan_duration);

  return result;
}

}  // namespace Features::Gallery::Scanner
