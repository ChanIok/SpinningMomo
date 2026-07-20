module;

module Features.Gallery.Scanner.Process;

import std;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Color.Types;
import Features.Gallery.Color.Extractor;
import Features.Gallery.Color.Repository;
import Features.Gallery.Folder.Service;
import Utils.Media.VideoAsset;
import Utils.Image;
import Utils.Logger;
import Utils.String;
import <wil/resource.h>;

namespace Features::Gallery::Scanner::Process {

// 处理单个文件：填 Asset 骨架 → 照片/视频元数据与缩略图 → 主色（仅照片）
auto process_single_file(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                         const Types::FileAnalysisResult& analysis,
                         const Types::ScanOptions& options,
                         const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                         Progress::ProcessingProgressTracker* progress_tracker)
    -> std::expected<ProcessedAssetEntry, std::string> {
  const auto& file_info = analysis.file_info;
  const auto& file_path = file_info.path;

  auto asset_type = Common::detect_asset_type(file_path);

  ProcessedAssetEntry processed;
  auto& asset = processed.asset;

  // 更新时保留原有 ID
  if (analysis.status == Types::FileStatus::MODIFIED && analysis.existing_metadata) {
    asset.id = analysis.existing_metadata->id;
  }

  asset.name = file_path.filename().string();
  asset.path = file_path.string();
  asset.type = asset_type;
  asset.size = file_info.size;

  if (file_path.has_extension()) {
    auto extension = Utils::String::ToLowerAscii(file_path.extension().string());
    asset.extension = extension;
  } else {
    asset.extension = std::nullopt;
  }

  asset.hash = file_info.hash.empty() ? std::nullopt : std::optional<std::string>{file_info.hash};
  asset.file_created_at = file_info.file_created_millis;
  asset.file_modified_at = file_info.file_modified_millis;

  // 父目录路径映射到 folder_id
  if (!folder_mapping.empty()) {
    auto parent_path = file_path.parent_path().string();
    if (auto it = folder_mapping.find(parent_path); it != folder_mapping.end()) {
      asset.folder_id = it->second;
    }
  }

  if (asset_type == "photo") {
    // 宽高 / mime
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

    std::optional<Utils::Image::BGRABitmapData> thumbnail_bitmap_data;

    // 缩略图像素同时供主色复用，避免同一照片重复解码缩放
    if (options.generate_thumbnails.value_or(true)) {
      auto bitmap_data_result = Utils::Image::load_scaled_bgra_bitmap_data(
          wic_factory.get(), file_path, options.thumbnail_short_edge.value_or(480));
      if (!bitmap_data_result) {
        Logger().warn("Failed to load thumbnail bitmap data for {}: {}", file_path.string(),
                      bitmap_data_result.error());
      } else {
        thumbnail_bitmap_data = std::move(bitmap_data_result.value());

        if (file_info.hash.empty()) {
          Logger().warn("Skip thumbnail generation for {}: empty hash", file_path.string());
        } else {
          auto thumbnail_result = Asset::Thumbnail::save_thumbnail_from_bgra(
              app_state, file_info.hash, thumbnail_bitmap_data.value(),
              options.rebuild_thumbnails.value_or(false));
          if (!thumbnail_result) {
            Logger().warn("Failed to generate thumbnail for {}: {}", file_path.string(),
                          thumbnail_result.error());
          }
        }
      }

      if (progress_tracker) {
        progress_tracker->mark_thumbnail_processed();
      }
    }

    const Features::Gallery::Color::Types::MainColorExtractOptions color_extract_options{
        .sample_short_edge = options.thumbnail_short_edge.value_or(480),
    };
    auto color_result = thumbnail_bitmap_data.has_value()
                            ? Features::Gallery::Color::Extractor::extract_main_colors_from_bgra(
                                  thumbnail_bitmap_data.value(), color_extract_options)
                            : Features::Gallery::Color::Extractor::extract_main_colors(
                                  wic_factory, file_path, color_extract_options);
    if (color_result) {
      processed.colors = std::move(color_result.value());
    } else {
      Logger().warn("Failed to extract main colors for {}: {}", file_path.string(),
                    color_result.error());
      processed.colors.clear();
    }
  } else if (asset_type == "video") {
    // MF：分辨率/时长 + 可选封面 WebP；视频不做主色
    auto video_result = Utils::Media::VideoAsset::analyze_video_file(
        file_path, options.generate_thumbnails.value_or(true)
                       ? std::optional<std::uint32_t>{options.thumbnail_short_edge.value_or(480)}
                       : std::nullopt);
    if (video_result) {
      asset.width = static_cast<std::int32_t>(video_result->width);
      asset.height = static_cast<std::int32_t>(video_result->height);
      asset.mime_type = video_result->mime_type;

      if (video_result->thumbnail.has_value()) {
        if (file_info.hash.empty()) {
          Logger().warn("Skip video thumbnail save for {}: empty hash", file_path.string());
        } else {
          auto save_result = Asset::Thumbnail::save_thumbnail_data(
              app_state, file_info.hash, *video_result->thumbnail,
              options.rebuild_thumbnails.value_or(false));
          if (!save_result) {
            Logger().warn("Failed to save video thumbnail for {}: {}", file_path.string(),
                          save_result.error());
          }
        }
      }
    } else {
      Logger().warn("Could not analyze video file {}: {}", file_path.string(),
                    video_result.error());
      asset.width = 0;
      asset.height = 0;
      asset.mime_type = "application/octet-stream";
    }

    if (options.generate_thumbnails.value_or(true) && progress_tracker) {
      progress_tracker->mark_thumbnail_processed();
    }

    processed.colors.clear();
  } else {
    asset.width = 0;
    asset.height = 0;
    asset.mime_type = "application/octet-stream";
    processed.colors.clear();
  }

  return processed;
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

          auto thread_wic_factory_result = Utils::Image::get_thread_wic_factory();
          if (!thread_wic_factory_result) {
            std::lock_guard<std::mutex> lock(results_mutex);
            final_result.errors.push_back("Failed to get thread WIC factory: " +
                                          thread_wic_factory_result.error());
            return;
          }
          auto thread_wic_factory = std::move(thread_wic_factory_result.value());

          FileProcessingBatchResult batch_result;

          for (size_t idx = start; idx < end; ++idx) {
            // 已开始的单文件媒体调用自然收尾，下一文件开始前响应停止
            if (stop_token.stop_requested()) {
              return;
            }

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

  // 批量插入新资产，并按插入 ID 写颜色
  if (!result.batch_result.new_assets.empty()) {
    std::vector<Types::Asset> new_assets_to_create;
    new_assets_to_create.reserve(result.batch_result.new_assets.size());
    for (const auto& entry : result.batch_result.new_assets) {
      new_assets_to_create.push_back(entry.asset);
    }

    auto create_result = Asset::Repository::batch_create_asset(app_state, new_assets_to_create);
    if (create_result) {
      Logger().info("Successfully created {} new asset items",
                    result.batch_result.new_assets.size());

      const auto& inserted_ids = create_result.value();
      if (inserted_ids.size() != result.batch_result.new_assets.size()) {
        Logger().error("Inserted asset count mismatch when replacing colors. assets={}, ids={}",
                       result.batch_result.new_assets.size(), inserted_ids.size());
        result.all_db_success = false;
      } else {
        std::vector<Features::Gallery::Color::Repository::ColorReplaceBatchItem> color_items;
        color_items.reserve(result.batch_result.new_assets.size());

        for (size_t i = 0; i < inserted_ids.size(); ++i) {
          color_items.push_back(Features::Gallery::Color::Repository::ColorReplaceBatchItem{
              .asset_id = inserted_ids[i],
              .colors = result.batch_result.new_assets[i].colors,
          });
        }

        auto color_result = Features::Gallery::Color::Repository::batch_replace_asset_colors(
            app_state, color_items);
        if (!color_result) {
          Logger().error("Failed to batch replace colors for new assets: {}", color_result.error());
          result.all_db_success = false;
        }
      }
    } else {
      Logger().error("Failed to batch create asset items: {}", create_result.error());
      result.all_db_success = false;
    }
  }

  // 批量更新已有资产与颜色
  if (!result.batch_result.updated_assets.empty()) {
    std::vector<Types::Asset> updated_assets_to_save;
    updated_assets_to_save.reserve(result.batch_result.updated_assets.size());
    for (const auto& entry : result.batch_result.updated_assets) {
      updated_assets_to_save.push_back(entry.asset);
    }

    auto update_result = Asset::Repository::batch_update_asset(app_state, updated_assets_to_save);
    if (update_result) {
      Logger().info("Successfully updated {} asset items",
                    result.batch_result.updated_assets.size());

      std::vector<Features::Gallery::Color::Repository::ColorReplaceBatchItem> color_items;
      color_items.reserve(result.batch_result.updated_assets.size());
      for (const auto& entry : result.batch_result.updated_assets) {
        if (entry.asset.id <= 0) {
          Logger().warn("Skip replacing colors for updated asset with invalid id: {}",
                        entry.asset.path);
          continue;
        }
        color_items.push_back(Features::Gallery::Color::Repository::ColorReplaceBatchItem{
            .asset_id = entry.asset.id,
            .colors = entry.colors,
        });
      }

      auto color_result =
          Features::Gallery::Color::Repository::batch_replace_asset_colors(app_state, color_items);
      if (!color_result) {
        Logger().error("Failed to batch replace colors for updated assets: {}",
                       color_result.error());
        result.all_db_success = false;
      }
    } else {
      Logger().error("Failed to batch update asset items: {}", update_result.error());
      result.all_db_success = false;
    }
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
