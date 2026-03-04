module;

module Features.Gallery.Scanner;

import std;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.ScanCommon;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Service;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Ignore.Service;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.Time;

namespace Features::Gallery::Scanner {

// 核心逻辑：扫描路径并应用过滤规则
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
    auto rules_result = Ignore::Service::load_ignore_rules(app_state, folder_id);
    if (!rules_result) {
      Logger().warn("Failed to load ignore rules: {}", rules_result.error());
    }
    auto combined_rules = rules_result.value_or(std::vector<Types::IgnoreRule>{});
    auto supported_extensions =
        options.supported_extensions.value_or(ScanCommon::default_supported_extensions());

    // 使用递归目录迭代器和 ranges 管道进行函数式过滤和转换
    // 整个流程惰性求值，直到最后的 to<std::vector> 才实际收集结果
    auto found_files =
        std::ranges::subrange(std::filesystem::recursive_directory_iterator(directory),
                              std::filesystem::recursive_directory_iterator{}) |
        // 步骤 1: 过滤掉不可访问的文件或跳过目录本身，只保留常规文件。捕获文件系统错误。
        std::views::filter([](const std::filesystem::directory_entry& entry) {
          try {
            return entry.is_regular_file();
          } catch (const std::filesystem::filesystem_error& e) {
            Logger().warn("Skipping file due to access error: {}", e.what());
            return false;
          }
        }) |
        // 步骤 2: 过滤扩展名。基于 options.supported_extensions 确定的列表（如 .jpg, .png）。
        std::views::filter([&supported_extensions](const std::filesystem::directory_entry& entry) {
          return ScanCommon::is_supported_file(entry.path(), supported_extensions);
        }) |
        // 步骤 3: 结合 .ignore 规则（类似 .gitignore）进行深层次忽略检查。
        std::views::filter(
            [&directory, &combined_rules](const std::filesystem::directory_entry& entry) {
              bool should_ignore =
                  Ignore::Service::apply_ignore_rules(entry.path(), directory, combined_rules);

              if (should_ignore) {
                Logger().debug("File ignored by rules: {}", entry.path().string());
              }

              return !should_ignore;
            }) |
        // 步骤 4: 提取符合条件的 std::filesystem::path 用于后续处理。
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

auto report_scan_progress(const std::function<void(const Types::ScanProgress&)>& progress_callback,
                          std::string stage, std::int64_t current, std::int64_t total,
                          std::optional<double> percent = std::nullopt,
                          std::optional<std::string> message = std::nullopt) -> void {
  if (!progress_callback) {
    return;
  }

  try {
    progress_callback(Types::ScanProgress{.stage = std::move(stage),
                                          .current = current,
                                          .total = total,
                                          .percent = percent,
                                          .message = std::move(message)});
  } catch (const std::exception& e) {
    Logger().warn("Scan progress callback failed: {}", e.what());
  }
}

auto steady_clock_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// 进度追踪器：支持加权计算和频率限制，防止 UI 阻塞
struct ProcessingProgressTracker {
  static constexpr std::int64_t kMinReportIntervalMillis = 200;

  const std::function<void(const Types::ScanProgress&)>& progress_callback;
  const std::int64_t total_files;
  const std::int64_t total_thumbnails;
  const std::int64_t total_units;
  const std::int64_t thumbnail_weight;
  const double percent_start;
  const double percent_end;

  std::atomic<std::int64_t> completed_files = 0;
  std::atomic<std::int64_t> completed_thumbnails = 0;
  std::atomic<std::int64_t> completed_units = 0;

  std::mutex report_mutex;
  int last_reported_percent = -1;
  std::int64_t last_report_millis = 0;

  ProcessingProgressTracker(const std::function<void(const Types::ScanProgress&)>& callback,
                            std::int64_t files, std::int64_t thumbnails, std::int64_t units,
                            std::int64_t thumbnail_weight_value, double start_percent,
                            double end_percent)
      : progress_callback(callback),
        total_files(files),
        total_thumbnails(thumbnails),
        total_units(units),
        thumbnail_weight(thumbnail_weight_value),
        percent_start(start_percent),
        percent_end(end_percent) {}

  auto report(bool force = false, std::optional<std::string> message = std::nullopt) -> void {
    if (!progress_callback || total_units <= 0) {
      return;
    }

    auto units_done = std::min(completed_units.load(std::memory_order_relaxed), total_units);
    if (force) {
      units_done = total_units;
    }

    const auto ratio = static_cast<double>(units_done) / static_cast<double>(total_units);
    auto percent = percent_start + (percent_end - percent_start) * ratio;
    percent = std::clamp(percent, percent_start, percent_end);

    const auto files_done = std::min(completed_files.load(std::memory_order_relaxed), total_files);
    const auto thumbnails_done =
        std::min(completed_thumbnails.load(std::memory_order_relaxed), total_thumbnails);

    auto now = steady_clock_millis();

    {
      std::lock_guard<std::mutex> lock(report_mutex);
      auto rounded_percent = static_cast<int>(std::floor(percent));

      if (!force) {
        if (rounded_percent <= last_reported_percent) {
          return;
        }

        if (now - last_report_millis < kMinReportIntervalMillis) {
          return;
        }
      }

      if (rounded_percent > last_reported_percent) {
        last_reported_percent = rounded_percent;
      }

      if (force && last_reported_percent >= 0 &&
          percent < static_cast<double>(last_reported_percent)) {
        percent = static_cast<double>(last_reported_percent);
      }

      last_report_millis = now;
    }

    if (!message.has_value()) {
      if (total_thumbnails > 0) {
        message = std::format("Processed {} / {} files, thumbnails {} / {}", files_done,
                              total_files, thumbnails_done, total_thumbnails);
      } else {
        message = std::format("Processed {} / {} files", files_done, total_files);
      }
    }

    report_scan_progress(progress_callback, "processing", units_done, total_units, percent,
                         std::move(message));
  }

  auto mark_file_processed() -> void {
    completed_files.fetch_add(1, std::memory_order_relaxed);
    completed_units.fetch_add(1, std::memory_order_relaxed);
    report();
  }

  auto mark_thumbnail_processed() -> void {
    if (total_thumbnails <= 0) {
      return;
    }

    completed_thumbnails.fetch_add(1, std::memory_order_relaxed);
    completed_units.fetch_add(thumbnail_weight, std::memory_order_relaxed);
    report();
  }
};

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

// 清理磁盘已删除但数据库仍存在的资产
auto cleanup_removed_assets(Core::State::AppState& app_state,
                            const std::filesystem::path& normalized_scan_root,
                            const std::vector<Types::FileSystemInfo>& file_infos,
                            const std::unordered_map<std::string, Types::Metadata>& asset_cache)
    -> int {
  auto root_str = normalized_scan_root.string();

  // 本次磁盘上实际存在的文件。
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

    if (!existing_paths.contains(cached_path)) {
      // DB 有但磁盘没有，后面要删掉。
      removed_assets.push_back(metadata);
    }
  }

  int deleted_count = 0;
  for (const auto& metadata : removed_assets) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, metadata.id);
    if (asset_result && asset_result->has_value()) {
      auto thumbnail_result = Asset::Thumbnail::delete_thumbnail(app_state, asset_result->value());
      if (!thumbnail_result) {
        Logger().debug("Thumbnail cleanup skipped for removed asset {}: {}", metadata.id,
                       thumbnail_result.error());
      }
    }

    auto delete_result = Asset::Repository::delete_asset(app_state, metadata.id);
    if (!delete_result) {
      Logger().warn("Failed to delete removed asset {}: {}", metadata.id, delete_result.error());
      continue;
    }

    deleted_count++;
  }

  if (deleted_count > 0) {
    Logger().info("Deleted {} removed assets under '{}'", deleted_count, root_str);
  }

  return deleted_count;
}

auto cleanup_missing_folders(Core::State::AppState& app_state,
                             const std::filesystem::path& normalized_scan_root) -> int {
  auto all_folders_result = Folder::Repository::list_all_folders(app_state);
  if (!all_folders_result) {
    Logger().warn("Failed to list folders for cleanup: {}", all_folders_result.error());
    return 0;
  }

  auto root_str = normalized_scan_root.string();

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

    std::error_code ec;
    auto folder_path = std::filesystem::path(folder.path);
    bool exists = std::filesystem::exists(folder_path, ec);
    bool is_directory = exists && std::filesystem::is_directory(folder_path, ec);
    if (ec || !exists || !is_directory) {
      // DB 有目录记录，但磁盘目录没了。
      missing_folders.push_back(MissingFolder{.id = folder.id, .path = folder.path});
    }
  }

  // 先删更深的目录，减少父子删除冲突。
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
    Logger().info("Deleted {} missing folders under '{}'", deleted_folders, root_str);
  }

  return deleted_folders;
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

    // 规范化路径，确保路径格式统一（统一使用正斜杠）
    auto normalized_path_result = Utils::Path::NormalizePath(file_path);
    if (!normalized_path_result) {
      Logger().warn("Failed to normalize path '{}': {}", file_path.string(),
                    normalized_path_result.error());
      continue;
    }

    Types::FileSystemInfo info{
        .path = normalized_path_result.value(),
        .size = static_cast<std::int64_t>(file_size),
        .file_modified_millis = Utils::Time::file_time_to_millis(last_write_time),
        .file_created_millis = creation_time_result.value(),
        .hash = ""};

    result.push_back(std::move(info));
  }

  Logger().info("Scanned {} files with ignore rules applied", result.size());
  return result;
}

// 状态分析：通过大小和修改时间初步判断文件状态
auto analyze_file_changes(const std::vector<Types::FileSystemInfo>& file_infos,
                          const std::unordered_map<std::string, Types::Metadata>& asset_cache)
    -> std::vector<Types::FileAnalysisResult> {
  std::vector<Types::FileAnalysisResult> results;
  results.reserve(file_infos.size());

  for (const auto& file_info : file_infos) {
    Types::FileAnalysisResult analysis;
    analysis.file_info = file_info;

    auto it = asset_cache.find(file_info.path.string());
    if (it == asset_cache.end()) {
      // 数据库中没有此路径记录，说明是全新文件，将在后续提取完整元数据。
      analysis.status = Types::FileStatus::NEW;
    } else {
      // 文件在数据库中有记录，但需进一步评估是否被修改过。
      const auto& cached_metadata = it->second;
      analysis.existing_metadata = cached_metadata;

      // 快速比较法：检查文件大小和系统修改时间。
      // 因为计算文件 Hash 代价昂贵，所以仅当尺寸或修改时间改变时才触发 NEEDS_HASH_CHECK。
      if (cached_metadata.size != file_info.size ||
          cached_metadata.file_modified_at != file_info.file_modified_millis) {
        analysis.status = Types::FileStatus::NEEDS_HASH_CHECK;
      } else {
        // 大小和修改时间均一致，极大可能未发生内容改变，跳过 Hash 计算直接标记为完毕。
        analysis.status = Types::FileStatus::UNCHANGED;
      }
    }

    results.push_back(std::move(analysis));
  }

  return results;
}

// 并行校检：对变动文件计算哈希，精准识别内容变化
auto calculate_hash_for_targets(Core::State::AppState& app_state,
                                std::vector<Types::FileAnalysisResult>& analysis_results)
    -> std::expected<void, std::string> {
  // 1. 使用 C++20 ranges 枚举并过滤出所有状态为 NEW 或 NEEDS_HASH_CHECK 的待处理文件。
  // 保留了原始索引(idx)，这是为了在并发算完哈希后能无缝写回原数组。
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

  // 使用 batches 把超大文件列表切分成每个容量32的小块，以供线程池粗粒度分发。
  constexpr size_t HASH_BATCH_SIZE = 32;
  auto batches =
      targets_with_index | std::views::chunk(HASH_BATCH_SIZE) | std::ranges::to<std::vector>();

  // std::latch 用于同步协调：等待所有子批次执行完毕后再释放控制流。
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

                    auto hash_result = ScanCommon::calculate_file_hash(analysis.file_info.path);
                    if (hash_result) {
                      return std::make_pair(idx, std::move(hash_result.value()));
                    } else {
                      Logger().warn("Failed to calculate hash for {}: {}",
                                    analysis.file_info.path.string(), hash_result.error());
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
                         const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                         ProcessingProgressTracker* progress_tracker)
    -> std::expected<Types::Asset, std::string> {
  const auto& file_info = analysis.file_info;
  const auto& file_path = file_info.path;

  auto asset_type = ScanCommon::detect_asset_type(file_path);

  Types::Asset asset;

  // 如果是更新，保留原有ID
  if (analysis.status == Types::FileStatus::MODIFIED && analysis.existing_metadata) {
    asset.id = analysis.existing_metadata->id;
  }

  asset.name = file_path.filename().string();
  asset.path = file_path.string();
  asset.type = asset_type;
  asset.size = file_info.size;

  // 设置文件扩展名（小写格式，包含点号）
  if (file_path.has_extension()) {
    auto extension = Utils::String::ToLowerAscii(file_path.extension().string());
    asset.extension = extension;
  } else {
    asset.extension = std::nullopt;
  }

  asset.hash = file_info.hash.empty() ? std::nullopt : std::optional<std::string>{file_info.hash};

  // 设置文件系统时间戳
  asset.file_created_at = file_info.file_created_millis;
  asset.file_modified_at = file_info.file_modified_millis;

  // 数据库记录管理时间戳由数据库自动设置

  // 根据文件路径查找最匹配的父级 folder_id
  if (!folder_mapping.empty()) {
    auto parent_path = file_path.parent_path().string();
    if (auto it = folder_mapping.find(parent_path); it != folder_mapping.end()) {
      asset.folder_id = it->second;
    }
  }

  // 针对图片资产(Photo)获取 WIC 特征
  if (asset_type == "photo") {
    // 解析具体的宽高、Mimetype，比如 1920x1080 image/png
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
      asset.mime_type = "application/octet-stream";  // 兜底处理
    }

    // 生成或更新这幅图的缩略图到缓存目录（用于无感滚动列表展示）
    if (options.generate_thumbnails.value_or(true)) {
      auto thumbnail_result =
          Asset::Thumbnail::generate_thumbnail(app_state, wic_factory, file_path, file_info.hash,
                                               options.thumbnail_short_edge.value_or(480));

      if (!thumbnail_result) {
        Logger().warn("Failed to generate thumbnail for {}: {}", file_path.string(),
                      thumbnail_result.error());
      }

      if (progress_tracker) {
        // 利用权重推进总体缩略图进度，保证进度条平滑增长
        progress_tracker->mark_thumbnail_processed();
      }
    }
  } else {
    // 非图片类型
    asset.width = 0;
    asset.height = 0;
    asset.mime_type = "application/octet-stream";
  }

  return asset;
}

// 并行处理：提取元数据并生成缩略图
auto process_files_in_parallel(Core::State::AppState& app_state,
                               const std::vector<Types::FileAnalysisResult>& files_to_process,
                               const Types::ScanOptions& options,
                               const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                               ProcessingProgressTracker* progress_tracker)
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
        *app_state.worker_pool,
        [&final_result, &results_mutex, &completion_latch, &app_state, &files_to_process, start,
         end, &options, &folder_mapping, progress_tracker]() {
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
                                                    options, folder_mapping, progress_tracker);
            if (asset_result) {
              if (analysis.status == Types::FileStatus::NEW) {
                batch_result.new_assets.push_back(std::move(asset_result.value()));
              } else if (analysis.status == Types::FileStatus::MODIFIED) {
                batch_result.updated_assets.push_back(std::move(asset_result.value()));
              }
            } else {
              batch_result.errors.push_back(
                  std::format("{}: {}", analysis.file_info.path.string(), asset_result.error()));
            }

            if (progress_tracker) {
              progress_tracker->mark_file_processed();
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

constexpr double kPreparingPercent = 2.0;
constexpr double kDiscoveringStartPercent = 10.0;
constexpr double kDiscoveringEndPercent = 25.0;
constexpr double kHashingStartPercent = 35.0;
constexpr double kHashingEndPercent = 60.0;
constexpr std::int64_t kThumbnailProgressWeight = 3;
constexpr double kProcessingStartPercent = 60.0;
constexpr double kProcessingEndPercent = 92.0;
constexpr double kCleanupPercent = 96.0;

struct ScanPreparationContext {
  std::filesystem::path normalized_scan_root;
  std::filesystem::path directory;
  std::int64_t folder_id = 0;
  std::unordered_map<std::string, Types::Metadata> asset_cache;
};

struct ProcessingPhaseResult {
  Types::ProcessingBatchResult batch_result;
  bool all_db_success = true;
};

// 准备阶段：规范化路径并加载缓存
auto prepare_scan_context(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<ScanPreparationContext, std::string> {
  auto normalized_scan_root_result = Utils::Path::NormalizePath(options.directory);
  if (!normalized_scan_root_result) {
    return std::unexpected("Failed to normalize scan root path: " +
                           normalized_scan_root_result.error());
  }
  auto normalized_scan_root = normalized_scan_root_result.value();

  Logger().info("Starting folder-aware asset scan for directory '{}' with {} ignore rules",
                normalized_scan_root.string(),
                options.ignore_rules.value_or(std::vector<Types::ScanIgnoreRule>{}).size());

  std::vector<std::filesystem::path> root_folder_paths = {normalized_scan_root};
  auto root_folder_mapping_result =
      Folder::Service::batch_create_folders_for_paths(app_state, root_folder_paths);
  if (!root_folder_mapping_result) {
    return std::unexpected("Failed to create root folder record: " +
                           root_folder_mapping_result.error());
  }

  auto root_folder_map = std::move(root_folder_mapping_result.value());
  std::int64_t folder_id = root_folder_map.at(normalized_scan_root.string());

  if (options.ignore_rules.has_value()) {
    auto persist_result = Ignore::Repository::replace_rules_by_folder_id(
        app_state, folder_id, options.ignore_rules.value());
    if (!persist_result) {
      return std::unexpected("Failed to persist ignore rules: " + persist_result.error());
    }
    Logger().info("Persisted {} ignore rules for folder_id {}", options.ignore_rules->size(),
                  folder_id);
  } else {
    Logger().debug("No ignore rules provided for '{}', keeping existing rules",
                   normalized_scan_root.string());
  }

  auto asset_cache_result = Asset::Service::load_asset_cache(app_state);
  if (!asset_cache_result) {
    return std::unexpected("Failed to load asset cache: " + asset_cache_result.error());
  }

  return ScanPreparationContext{
      .normalized_scan_root = normalized_scan_root,
      .directory = normalized_scan_root,
      .folder_id = folder_id,
      .asset_cache = std::move(asset_cache_result.value()),
  };
}

auto run_discovery_phase(Core::State::AppState& app_state, const ScanPreparationContext& context,
                         const Types::ScanOptions& options,
                         const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string> {
  report_scan_progress(progress_callback, "discovering", 0, 1, kDiscoveringStartPercent,
                       "Scanning files from disk");

  auto file_info_result = scan_file_info(app_state, context.directory, options, context.folder_id);
  if (!file_info_result) {
    return std::unexpected("File info scanning failed: " + file_info_result.error());
  }

  auto file_infos = std::move(file_info_result.value());
  report_scan_progress(progress_callback, "discovering",
                       static_cast<std::int64_t>(file_infos.size()),
                       static_cast<std::int64_t>(file_infos.size()), kDiscoveringEndPercent,
                       std::format("Discovered {} candidate files", file_infos.size()));

  Logger().info("Scanned {} files in directory '{}' (after ignore rules)", file_infos.size(),
                context.normalized_scan_root.string());
  return file_infos;
}

auto run_hash_analysis_phase(
    Core::State::AppState& app_state, const std::vector<Types::FileSystemInfo>& file_infos,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache,
    const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<std::vector<Types::FileAnalysisResult>, std::string> {
  auto analysis_results = analyze_file_changes(file_infos, asset_cache);

  report_scan_progress(progress_callback, "hashing", 0,
                       static_cast<std::int64_t>(analysis_results.size()), kHashingStartPercent,
                       "Calculating file hashes");

  if (auto hash_phase = calculate_hash_for_targets(app_state, analysis_results); !hash_phase) {
    return std::unexpected("Hash calculation failed: " + hash_phase.error());
  }

  report_scan_progress(progress_callback, "hashing",
                       static_cast<std::int64_t>(analysis_results.size()),
                       static_cast<std::int64_t>(analysis_results.size()), kHashingEndPercent,
                       "Hash calculation completed");

  Logger().info("Calculated hashes for {} files", analysis_results.size());

  std::vector<Types::FileAnalysisResult> files_to_process;
  std::ranges::copy_if(analysis_results, std::back_inserter(files_to_process),
                       [](const Types::FileAnalysisResult& result) {
                         return result.status == Types::FileStatus::NEW ||
                                result.status == Types::FileStatus::MODIFIED;
                       });

  Logger().info("Found {} files that need processing", files_to_process.size());
  return files_to_process;
}

auto run_processing_phase(Core::State::AppState& app_state, const std::filesystem::path& directory,
                          const std::vector<Types::FileAnalysisResult>& files_to_process,
                          const Types::ScanOptions& options,
                          const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<ProcessingPhaseResult, std::string> {
  std::int64_t thumbnail_targets = 0;
  if (options.generate_thumbnails.value_or(true)) {
    thumbnail_targets = static_cast<std::int64_t>(
        std::ranges::count_if(files_to_process, [](const Types::FileAnalysisResult& result) {
          return ScanCommon::is_photo_file(result.file_info.path);
        }));
  }

  std::int64_t processing_total_units = static_cast<std::int64_t>(files_to_process.size()) +
                                        thumbnail_targets * kThumbnailProgressWeight;

  auto processing_start_message =
      thumbnail_targets > 0 ? std::format("Processing {} changed files ({} thumbnails)",
                                          files_to_process.size(), thumbnail_targets)
                            : std::format("Processing {} changed files", files_to_process.size());

  report_scan_progress(progress_callback, "processing", 0, processing_total_units,
                       kProcessingStartPercent, std::move(processing_start_message));

  ProcessingPhaseResult result{};
  if (files_to_process.empty()) {
    Logger().info("Folder-aware asset scan found no new or modified files");
    report_scan_progress(progress_callback, "processing", 0, 0, kProcessingEndPercent,
                         "No changed files found");
    return result;
  }

  std::vector<std::filesystem::path> file_paths;
  file_paths.reserve(files_to_process.size());
  for (const auto& analysis : files_to_process) {
    file_paths.push_back(analysis.file_info.path);
  }

  std::unordered_map<std::string, std::int64_t> path_to_folder_id;
  auto folder_paths = Folder::Service::extract_unique_folder_paths(file_paths, directory);
  auto folder_mapping_result =
      Folder::Service::batch_create_folders_for_paths(app_state, folder_paths);
  if (folder_mapping_result) {
    path_to_folder_id = std::move(folder_mapping_result.value());
    Logger().info("Pre-created {} folder records with complete hierarchy",
                  path_to_folder_id.size());
  } else {
    Logger().warn("Failed to pre-create folders: {}", folder_mapping_result.error());
  }

  std::optional<ProcessingProgressTracker> processing_tracker;
  if (processing_total_units > 0) {
    processing_tracker.emplace(progress_callback,
                               static_cast<std::int64_t>(files_to_process.size()),
                               thumbnail_targets, processing_total_units, kThumbnailProgressWeight,
                               kProcessingStartPercent, kProcessingEndPercent);
  }

  auto processing_result =
      process_files_in_parallel(app_state, files_to_process, options, path_to_folder_id,
                                processing_tracker ? &(*processing_tracker) : nullptr);
  if (!processing_result) {
    return std::unexpected("File processing failed: " + processing_result.error());
  }

  result.batch_result = std::move(processing_result.value());

  if (!result.batch_result.new_assets.empty()) {
    auto create_result =
        Asset::Repository::batch_create_asset(app_state, result.batch_result.new_assets);
    if (create_result) {
      Logger().info("Successfully created {} new asset items",
                    result.batch_result.new_assets.size());
    } else {
      Logger().error("Failed to batch create asset items: {}", create_result.error());
      result.all_db_success = false;
    }
  }

  if (!result.batch_result.updated_assets.empty()) {
    auto update_result =
        Asset::Repository::batch_update_asset(app_state, result.batch_result.updated_assets);
    if (update_result) {
      Logger().info("Successfully updated {} asset items",
                    result.batch_result.updated_assets.size());
    } else {
      Logger().error("Failed to batch update asset items: {}", update_result.error());
      result.all_db_success = false;
    }
  }

  if (processing_tracker) {
    processing_tracker->report(true, "File processing completed");
  } else {
    report_scan_progress(progress_callback, "processing", processing_total_units,
                         processing_total_units, kProcessingEndPercent,
                         "File processing completed");
  }

  return result;
}

auto run_cleanup_phase(Core::State::AppState& app_state,
                       const std::filesystem::path& normalized_scan_root,
                       const std::vector<Types::FileSystemInfo>& file_infos,
                       const std::unordered_map<std::string, Types::Metadata>& asset_cache,
                       const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> int {
  report_scan_progress(progress_callback, "cleanup", 0, 1, kCleanupPercent,
                       "Reconciling deleted files");

  int deleted_items =
      cleanup_removed_assets(app_state, normalized_scan_root, file_infos, asset_cache);
  [[maybe_unused]] int deleted_folders = cleanup_missing_folders(app_state, normalized_scan_root);
  return deleted_items;
}

// =============================================================================
// 主扫描入口：协调五个阶段完成资产同步
// =============================================================================
auto scan_asset_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                          std::function<void(const Types::ScanProgress&)> progress_callback)
    -> std::expected<Types::ScanResult, std::string> {
  auto start_time = std::chrono::steady_clock::now();
  report_scan_progress(progress_callback, "preparing", 0, 1, kPreparingPercent,
                       "Preparing gallery scan context");

  // 步骤 1: 准备阶段 (获取规范化路径，预先载入数据库中的资产缓存)
  auto context_result = prepare_scan_context(app_state, options);
  if (!context_result) {
    return std::unexpected(context_result.error());
  }
  auto context = std::move(context_result.value());

  // 步骤 2: 发现阶段 (遍历磁盘系统，按照扩展名和忽略规则筛选文件)
  auto file_infos_result = run_discovery_phase(app_state, context, options, progress_callback);
  if (!file_infos_result) {
    return std::unexpected(file_infos_result.error());
  }
  auto file_infos = std::move(file_infos_result.value());

  // 步骤 3: 哈希分析阶段 (对比缓存，对于变动的文件进行哈希校检来确定状态)
  auto files_to_process_result =
      run_hash_analysis_phase(app_state, file_infos, context.asset_cache, progress_callback);
  if (!files_to_process_result) {
    return std::unexpected(files_to_process_result.error());
  }
  auto files_to_process = std::move(files_to_process_result.value());

  // 步骤 4: 处理阶段 (提取全新或发生修改的文件的元信息，并生成缩略图)
  auto processing_result = run_processing_phase(app_state, context.directory, files_to_process,
                                                options, progress_callback);
  if (!processing_result) {
    return std::unexpected(processing_result.error());
  }
  auto processing_phase = std::move(processing_result.value());

  // 步骤 5: 清理阶段 (从数据库中移除那些在本次磁盘扫描中已经不存在的资产)
  int deleted_items = run_cleanup_phase(app_state, context.normalized_scan_root, file_infos,
                                        context.asset_cache, progress_callback);

  Types::ScanResult result{
      .total_files = static_cast<int>(file_infos.size()),
      .new_items = static_cast<int>(processing_phase.batch_result.new_assets.size()),
      .updated_items = static_cast<int>(processing_phase.batch_result.updated_assets.size()),
      .deleted_items = deleted_items,
      .errors = std::move(processing_phase.batch_result.errors),
  };

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.scan_duration = std::format("{}ms", duration.count());

  if (!processing_phase.all_db_success) {
    result.errors.push_back("Some database operations failed");
  }

  Logger().info(
      "Folder-aware asset scan completed. Total: {}, New: {}, Updated: {}, Deleted: {}, Errors: "
      "{}, Duration: {}",
      result.total_files, result.new_items, result.updated_items, result.deleted_items,
      result.errors.size(), result.scan_duration);

  report_scan_progress(
      progress_callback, "completed", static_cast<std::int64_t>(result.total_files),
      static_cast<std::int64_t>(result.total_files), 100.0, "Gallery scan completed");

  return result;
}

}  // namespace Features::Gallery::Scanner
