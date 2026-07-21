module;

module Features.Gallery.Scanner.Process;

import std;
import Core.Database;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Scanner.AssetPipeline;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Color.Repository;
import Features.Gallery.Folder.Service;
import Utils.Logger;
import <wil/resource.h>;

namespace Features::Gallery::Scanner::Process {

// 处理单个文件：通过 AssetPipeline 物化媒体，并推进缩略图进度
auto process_single_file(Core::State::AppState& app_state,
                         const Types::FileAnalysisResult& analysis,
                         const Types::ScanOptions& options,
                         const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                         Progress::ProcessingProgressTracker* progress_tracker)
    -> std::expected<ProcessedAssetEntry, std::string> {
  const auto& file_info = analysis.file_info;
  const auto& file_path = file_info.path;

  std::optional<std::int64_t> folder_id;
  if (!folder_mapping.empty()) {
    auto parent_path = file_path.parent_path().string();
    if (auto it = folder_mapping.find(parent_path); it != folder_mapping.end()) {
      folder_id = it->second;
    }
  }

  AssetPipeline::MediaPrepareInput input{
      .hash = file_info.hash,
      .size = file_info.size,
      .file_created_millis = file_info.file_created_millis,
      .file_modified_millis = file_info.file_modified_millis,
      .folder_id = folder_id,
      // metadata cache 已有更新定位所需的 ID，不再逐文件回读完整 Asset。
      .existing_asset_id = analysis.existing_metadata
                               ? std::optional<std::int64_t>{analysis.existing_metadata->id}
                               : std::nullopt,
  };

  auto prepared_result = AssetPipeline::prepare_media_asset(app_state, file_path, options, input);
  if (!prepared_result) {
    return std::unexpected(prepared_result.error());
  }

  auto asset_type = Common::detect_asset_type(file_path);
  if (options.generate_thumbnails.value_or(true) && progress_tracker &&
      (asset_type == "photo" || asset_type == "video")) {
    progress_tracker->mark_thumbnail_processed();
  }

  return ProcessedAssetEntry{
      .asset = std::move(prepared_result->asset),
      .colors = std::move(prepared_result->colors),
  };
}

// 线程池分批并行处理文件，合并 NEW/MODIFIED 结果
auto process_files_in_parallel(Core::State::AppState& app_state,
                               const std::vector<Types::FileAnalysisResult>& files_to_process,
                               const Types::ScanOptions& options,
                               const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                               Progress::ProcessingProgressTracker* progress_tracker,
                               std::stop_token stop_token)
    -> std::expected<FileProcessingBatchResult, std::string> {
  if (files_to_process.empty()) {
    return FileProcessingBatchResult{};
  }

  constexpr size_t PROCESS_BATCH_SIZE = 16;
  size_t total_batches = (files_to_process.size() + PROCESS_BATCH_SIZE - 1) / PROCESS_BATCH_SIZE;

  std::latch completion_latch(total_batches);
  FileProcessingBatchResult final_result;
  std::mutex results_mutex;
  std::size_t submitted_batches = 0;

  for (size_t batch_idx = 0; batch_idx < total_batches; ++batch_idx) {
    size_t start = batch_idx * PROCESS_BATCH_SIZE;
    size_t end = std::min(start + PROCESS_BATCH_SIZE, files_to_process.size());

    bool submitted = Core::WorkerPool::submit_task(
        app_state, [&final_result, &results_mutex, &completion_latch, &app_state, &files_to_process,
                    start, end, &options, &folder_mapping, progress_tracker, stop_token]() {
          auto finish_batch =
              wil::scope_exit([&completion_latch] { completion_latch.count_down(); });

          FileProcessingBatchResult batch_result;

          for (size_t idx = start; idx < end; ++idx) {
            // 已开始的单文件媒体调用自然收尾，下一文件开始前响应停止
            if (stop_token.stop_requested()) {
              return;
            }

            const auto& analysis = files_to_process[idx];
            auto asset_result =
                process_single_file(app_state, analysis, options, folder_mapping, progress_tracker);
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
        });

    if (!submitted) {
      completion_latch.count_down(static_cast<std::ptrdiff_t>(total_batches - submitted_batches));
      completion_latch.wait();
      return std::unexpected("Failed to submit file processing task to worker pool");
    }
    submitted_batches++;
  }

  completion_latch.wait();

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  return final_result;
}

// 处理阶段：建 folder 映射 → 并行抽元数据/缩略图/主色 → 批量写库与颜色
auto run_processing_phase(Core::State::AppState& app_state, const std::filesystem::path& directory,
                          const std::vector<Types::FileAnalysisResult>& files_to_process,
                          const Types::ScanOptions& options,
                          const std::function<void(const Types::ScanProgress&)>& progress_callback,
                          std::stop_token stop_token)
    -> std::expected<ProcessingPhaseResult, std::string> {
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  std::int64_t thumbnail_targets = 0;
  if (options.generate_thumbnails.value_or(true)) {
    thumbnail_targets = static_cast<std::int64_t>(
        std::ranges::count_if(files_to_process, [](const Types::FileAnalysisResult& result) {
          auto asset_type = Common::detect_asset_type(result.file_info.path);
          // 视频与照片一样计入缩略图任务，封面失败时仍 mark，避免进度条卡住
          return asset_type == "photo" || asset_type == "video";
        }));
  }

  std::int64_t processing_total_units = static_cast<std::int64_t>(files_to_process.size()) +
                                        thumbnail_targets * Progress::kThumbnailProgressWeight;

  auto processing_start_message =
      thumbnail_targets > 0 ? std::format("Processing {} changed files ({} thumbnails)",
                                          files_to_process.size(), thumbnail_targets)
                            : std::format("Processing {} changed files", files_to_process.size());

  Progress::report_scan_progress(progress_callback, "processing", 0, processing_total_units,
                                 Progress::kProcessingStartPercent,
                                 std::move(processing_start_message));

  ProcessingPhaseResult result{};
  if (files_to_process.empty()) {
    Logger().info("Folder-aware asset scan found no new or modified files");
    Progress::report_scan_progress(progress_callback, "processing", 0, 0,
                                   Progress::kProcessingEndPercent, "No changed files found");
    return result;
  }

  // 为待处理文件预建完整 folder 层级
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

  std::optional<Progress::ProcessingProgressTracker> processing_tracker;
  if (processing_total_units > 0) {
    processing_tracker.emplace(
        progress_callback, static_cast<std::int64_t>(files_to_process.size()), thumbnail_targets,
        processing_total_units, Progress::kThumbnailProgressWeight,
        Progress::kProcessingStartPercent, Progress::kProcessingEndPercent);
  }

  auto processing_result =
      process_files_in_parallel(app_state, files_to_process, options, path_to_folder_id,
                                processing_tracker ? &(*processing_tracker) : nullptr, stop_token);
  if (!processing_result) {
    return std::unexpected("File processing failed: " + processing_result.error());
  }

  result.batch_result = std::move(processing_result.value());

  // 新建、更新与颜色替换共用一个事务，任一颜色写入失败都会回滚对应资产指纹。
  if (!result.batch_result.new_assets.empty() || !result.batch_result.updated_assets.empty()) {
    auto persist_result = Core::Database::execute_transaction(
        app_state,
        [&result](Core::State::AppState& txn_app_state) -> std::expected<void, std::string> {
          for (auto& entry : result.batch_result.new_assets) {
            auto create_result = Asset::Repository::create_asset(txn_app_state, entry.asset);
            if (!create_result) {
              return std::unexpected("Failed to create asset: " + create_result.error());
            }
            entry.asset.id = create_result.value();

            auto color_result =
                Features::Gallery::Color::Repository::replace_asset_colors_in_transaction(
                    txn_app_state, entry.asset.id, entry.colors);
            if (!color_result) {
              return std::unexpected("Failed to create asset colors: " + color_result.error());
            }
          }

          for (const auto& entry : result.batch_result.updated_assets) {
            if (entry.asset.id <= 0) {
              return std::unexpected("Invalid asset id while updating: " + entry.asset.path);
            }

            auto update_result =
                Asset::Repository::update_asset_scanner_fields(txn_app_state, entry.asset);
            if (!update_result) {
              return std::unexpected("Failed to update asset: " + update_result.error());
            }

            auto color_result =
                Features::Gallery::Color::Repository::replace_asset_colors_in_transaction(
                    txn_app_state, entry.asset.id, entry.colors);
            if (!color_result) {
              return std::unexpected("Failed to update asset colors: " + color_result.error());
            }
          }
          return {};
        });

    if (!persist_result) {
      // 原子写入失败后立即终止全量扫描，避免继续清理或发布并未落库的变化。
      return std::unexpected("Failed to persist scanned assets and colors atomically: " +
                             persist_result.error());
    }

    Logger().info("Successfully created {} and updated {} asset items with colors",
                  result.batch_result.new_assets.size(), result.batch_result.updated_assets.size());
  }

  if (processing_tracker) {
    processing_tracker->report(true, "File processing completed");
  } else {
    Progress::report_scan_progress(progress_callback, "processing", processing_total_units,
                                   processing_total_units, Progress::kProcessingEndPercent,
                                   "File processing completed");
  }

  return result;
}

}  // namespace Features::Gallery::Scanner::Process
