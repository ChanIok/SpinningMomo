module;

module Features.Gallery.Scanner.Analysis;

import std;
import Core.State;
import Core.WorkerPool;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Asset.Repository;
import Utils.Logger;
import <wil/resource.h>;

namespace Features::Gallery::Scanner::Analysis {

// 用 size/mtime（及 force）把文件标成 NEW / NEEDS_HASH_CHECK / UNCHANGED
auto analyze_file_changes(const std::vector<Types::FileSystemInfo>& file_infos,
                          const std::unordered_map<std::string, Types::Metadata>& asset_cache,
                          bool force_reanalyze) -> std::vector<Types::FileAnalysisResult> {
  std::vector<Types::FileAnalysisResult> results;
  results.reserve(file_infos.size());

  for (const auto& file_info : file_infos) {
    Types::FileAnalysisResult analysis;
    analysis.file_info = file_info;

    auto it = asset_cache.find(file_info.path.string());
    if (it == asset_cache.end()) {
      // 库中无此路径 → 全新文件
      analysis.status = Types::FileStatus::NEW;
    } else {
      const auto& cached_metadata = it->second;
      analysis.existing_metadata = cached_metadata;

      if (force_reanalyze) {
        // 强制重分析也先算指纹，避免后续缩略图流程拿到空 hash
        analysis.status = Types::FileStatus::NEEDS_HASH_CHECK;
        results.push_back(std::move(analysis));
        continue;
      }

      // 缺指纹或 size/mtime 变化才重算，正常扫描避免重复读媒体
      if (cached_metadata.hash.empty() || cached_metadata.size != file_info.size ||
          cached_metadata.file_modified_at != file_info.file_modified_millis) {
        analysis.status = Types::FileStatus::NEEDS_HASH_CHECK;
      } else {
        analysis.status = Types::FileStatus::UNCHANGED;
      }
    }

    results.push_back(std::move(analysis));
  }

  return results;
}

// 并行计算 NEW / NEEDS_HASH_CHECK 的内容指纹，并写回 analysis 状态
auto calculate_hash_for_targets(Core::State::AppState& app_state,
                                std::vector<Types::FileAnalysisResult>& analysis_results,
                                Progress::HashProgressTracker* progress_tracker,
                                std::stop_token stop_token)
    -> std::expected<std::size_t, std::string> {
  // 保留原始下标，便于并发写回
  auto targets_with_index = analysis_results | std::views::enumerate |
                            std::views::filter([](const auto& pair) {
                              const auto& [idx, analysis] = pair;
                              return analysis.status == Types::FileStatus::NEW ||
                                     analysis.status == Types::FileStatus::NEEDS_HASH_CHECK;
                            }) |
                            std::ranges::to<std::vector>();

  if (targets_with_index.empty()) {
    return 0;
  }

  constexpr size_t HASH_BATCH_SIZE = 32;
  auto batches =
      targets_with_index | std::views::chunk(HASH_BATCH_SIZE) | std::ranges::to<std::vector>();

  std::latch completion_latch(batches.size());
  std::vector<std::pair<size_t, std::string>> all_hashes;
  std::mutex results_mutex;
  std::size_t submitted_batches = 0;

  for (const auto& batch : batches) {
    bool submitted = Core::WorkerPool::submit_task(
        app_state, [&all_hashes, &results_mutex, &completion_latch, batch, &analysis_results,
                    progress_tracker, stop_token]() {
          auto finish_batch =
              wil::scope_exit([&completion_latch] { completion_latch.count_down(); });

          auto batch_hashes =
              batch | std::views::take_while([stop_token](const auto&) {
                return !stop_token.stop_requested();
              }) |
              std::views::transform([progress_tracker, stop_token](const auto& pair)
                                        -> std::optional<std::pair<size_t, std::string>> {
                const auto& [idx, analysis] = pair;
                auto hash_result = Common::calculate_content_fingerprint(
                    analysis.file_info.path, analysis.file_info.size, stop_token);
                if (progress_tracker) {
                  progress_tracker->mark_item_hashed();
                }
                if (hash_result) {
                  return std::make_pair(static_cast<size_t>(idx), std::move(hash_result.value()));
                }
                if (!stop_token.stop_requested()) {
                  Logger().warn("Failed to calculate hash for {}: {}",
                                analysis.file_info.path.string(), hash_result.error());
                }
                return std::nullopt;
              }) |
              std::views::filter([](const auto& result) { return result.has_value(); }) |
              std::views::transform([](auto&& result) { return std::move(result.value()); }) |
              std::ranges::to<std::vector>();

          if (!batch_hashes.empty()) {
            std::lock_guard<std::mutex> lock(results_mutex);
            all_hashes.insert(all_hashes.end(), std::make_move_iterator(batch_hashes.begin()),
                              std::make_move_iterator(batch_hashes.end()));
          }
        });

    if (!submitted) {
      // 已提交批次仍持有局部引用，必须等它们收尾
      completion_latch.count_down(static_cast<std::ptrdiff_t>(batches.size() - submitted_batches));
      completion_latch.wait();
      return std::unexpected("Failed to submit hash calculation task to worker pool");
    }
    submitted_batches++;
  }

  completion_latch.wait();

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 指纹结果写回：内容未变则只更新 size/mtime，内容变了标 MODIFIED
  for (const auto& hash_pair : all_hashes) {
    const auto& [idx, hash] = hash_pair;
    auto& analysis = analysis_results[idx];
    analysis.file_info.hash = hash;

    if (analysis.status == Types::FileStatus::NEEDS_HASH_CHECK) {
      const bool hash_unchanged = analysis.existing_metadata &&
                                  !analysis.existing_metadata->hash.empty() &&
                                  analysis.existing_metadata->hash == hash;

      if (hash_unchanged) {
        // 内容未变也要写回最新 size/mtime，否则下次扫描会再次算同一指纹
        auto update_result = Asset::Repository::update_asset_file_state(
            app_state, analysis.existing_metadata->id, analysis.file_info.size,
            analysis.file_info.file_modified_millis);
        if (!update_result) {
          return std::unexpected(update_result.error());
        }
      }

      analysis.status = hash_unchanged ? Types::FileStatus::UNCHANGED : Types::FileStatus::MODIFIED;
    }
  }

  return all_hashes.size();
}

// 指纹分析阶段：size/mtime 粗判 → 并行内容指纹 → 产出 NEW/MODIFIED 待处理列表
auto run_hash_analysis_phase(
    Core::State::AppState& app_state, const std::vector<Types::FileSystemInfo>& file_infos,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache,
    const Types::ScanOptions& options,
    const std::function<void(const Types::ScanProgress&)>& progress_callback,
    std::stop_token stop_token)
    -> std::expected<std::vector<Types::FileAnalysisResult>, std::string> {
  auto analysis_results =
      analyze_file_changes(file_infos, asset_cache, options.force_reanalyze.value_or(false));

  const auto hash_candidate_count = static_cast<std::size_t>(
      std::ranges::count_if(analysis_results, [](const Types::FileAnalysisResult& analysis) {
        return analysis.status == Types::FileStatus::NEW ||
               analysis.status == Types::FileStatus::NEEDS_HASH_CHECK;
      }));
  const auto metadata_unchanged_skip_count = analysis_results.size() - hash_candidate_count;

  std::optional<Progress::HashProgressTracker> hash_tracker;
  if (hash_candidate_count > 0) {
    hash_tracker.emplace(progress_callback, static_cast<std::int64_t>(hash_candidate_count),
                         Progress::kHashingStartPercent, Progress::kHashingEndPercent);
    hash_tracker->report(false);
  } else {
    Progress::report_scan_progress(progress_callback, "hashing", 0, 0,
                                   Progress::kHashingStartPercent,
                                   "No files require fingerprint calculation");
  }

  auto hash_phase = calculate_hash_for_targets(
      app_state, analysis_results, hash_tracker ? &(*hash_tracker) : nullptr, stop_token);
  if (!hash_phase) {
    return std::unexpected("Fingerprint calculation failed: " + hash_phase.error());
  }
  const std::size_t hashed_count = hash_phase.value();

  // force_reanalyze：有 existing 的一律按 MODIFIED 进入后续处理
  if (options.force_reanalyze.value_or(false)) {
    for (auto& analysis : analysis_results) {
      if (analysis.existing_metadata.has_value()) {
        analysis.status = Types::FileStatus::MODIFIED;
      }
    }
  }

  if (hash_tracker) {
    hash_tracker->report(true);
  } else {
    Progress::report_scan_progress(
        progress_callback, "hashing", static_cast<std::int64_t>(hashed_count),
        static_cast<std::int64_t>(hash_candidate_count), Progress::kHashingEndPercent,
        "Fingerprint calculation completed");
  }

  if (hash_candidate_count == 0) {
    Logger().info("Fingerprint calculation skipped for all {} files (size and mtime match cache)",
                  analysis_results.size());
  } else {
    const auto hash_failures = hash_candidate_count - hashed_count;
    if (hash_failures == 0) {
      Logger().info("Calculated hashes for {} of {} files ({} skipped by unchanged size/mtime)",
                    hashed_count, analysis_results.size(), metadata_unchanged_skip_count);
    } else {
      Logger().warn(
          "Calculated hashes for {} of {} candidate files ({} skipped by unchanged size/mtime, "
          "{} failed)",
          hashed_count, analysis_results.size(), metadata_unchanged_skip_count, hash_failures);
    }
  }

  std::vector<Types::FileAnalysisResult> files_to_process;
  std::ranges::copy_if(analysis_results, std::back_inserter(files_to_process),
                       [](const Types::FileAnalysisResult& result) {
                         return result.status == Types::FileStatus::NEW ||
                                result.status == Types::FileStatus::MODIFIED;
                       });

  Logger().info("Found {} files that need processing", files_to_process.size());
  return files_to_process;
}

}  // namespace Features::Gallery::Scanner::Analysis
