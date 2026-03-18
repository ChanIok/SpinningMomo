module;

#include <asio.hpp>

module Extensions.InfinityNikki.PhotoExtract;

import std;
import Core.State;
import Extensions.InfinityNikki.PhotoExtract.Infra;
import Extensions.InfinityNikki.PhotoExtract.Scan;
import Extensions.InfinityNikki.Types;
import Utils.Logger;

namespace Extensions::InfinityNikki::PhotoExtract {

constexpr std::size_t kMaxErrorMessages = 50;
constexpr std::size_t kExtractBatchSize = 32;
constexpr std::int64_t kMinProgressReportIntervalMillis = 200;
constexpr double kPreparingPercent = 2.0;
constexpr double kProcessingStartPercent = 5.0;
constexpr double kProcessingEndPercent = 99.0;

struct ExtractProgressState {
  std::int64_t total_candidates = 0;
  std::int64_t scanned_count = 0;
  std::int64_t finalized_count = 0;
  int last_reported_percent = -1;
  std::int64_t last_report_millis = 0;
};

auto add_error(std::vector<std::string>& errors, std::string message) -> void {
  if (errors.size() >= kMaxErrorMessages) {
    return;
  }
  errors.push_back(std::move(message));
}

auto steady_clock_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

auto report_extract_progress(
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    std::string stage, std::int64_t current, std::int64_t total,
    std::optional<double> percent = std::nullopt, std::optional<std::string> message = std::nullopt)
    -> void {
  if (!progress_callback) {
    return;
  }

  if (!percent.has_value() && total > 0) {
    percent = (static_cast<double>(current) * 100.0) / static_cast<double>(total);
  }

  if (percent.has_value()) {
    percent = std::clamp(*percent, 0.0, 100.0);
  }

  progress_callback(InfinityNikkiExtractPhotoParamsProgress{
      .stage = std::move(stage),
      .current = current,
      .total = total,
      .percent = percent,
      .message = std::move(message),
  });
}

auto calculate_processing_percent(const ExtractProgressState& progress) -> double {
  if (progress.total_candidates <= 0) {
    return 100.0;
  }

  auto total = static_cast<double>(progress.total_candidates);
  auto scanned_ratio = std::clamp(static_cast<double>(progress.scanned_count) / total, 0.0, 1.0);
  auto finalized_ratio =
      std::clamp(static_cast<double>(progress.finalized_count) / total, 0.0, 1.0);
  auto overall_ratio = (scanned_ratio + finalized_ratio) / 2.0;

  return kProcessingStartPercent +
         overall_ratio * (kProcessingEndPercent - kProcessingStartPercent);
}

auto build_processing_message(const ExtractProgressState& progress,
                              const InfinityNikkiExtractPhotoParamsResult& result) -> std::string {
  return std::format("Scanned {} / {}, finalized {} / {}, saved {}, skipped {}, failed {}",
                     progress.scanned_count, progress.total_candidates, progress.finalized_count,
                     progress.total_candidates, result.saved_count, result.skipped_count,
                     result.failed_count);
}

auto report_processing_progress(
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    ExtractProgressState& progress, const InfinityNikkiExtractPhotoParamsResult& result,
    bool force = false, std::optional<std::string> message = std::nullopt) -> void {
  if (!progress_callback || progress.total_candidates <= 0) {
    return;
  }

  auto percent = calculate_processing_percent(progress);
  auto rounded_percent = static_cast<int>(std::floor(percent));
  auto now = steady_clock_millis();

  if (!force) {
    if (rounded_percent <= progress.last_reported_percent) {
      return;
    }

    if (now - progress.last_report_millis < kMinProgressReportIntervalMillis) {
      return;
    }
  }

  progress.last_reported_percent = std::max(progress.last_reported_percent, rounded_percent);
  progress.last_report_millis = now;

  if (!message.has_value()) {
    message = build_processing_message(progress, result);
  }

  report_extract_progress(progress_callback, "processing", progress.finalized_count,
                          progress.total_candidates, percent, std::move(message));
}

auto mark_candidate_skipped(InfinityNikkiExtractPhotoParamsResult& result,
                            ExtractProgressState& progress, std::int64_t asset_id,
                            const std::string& reason) -> void {
  result.skipped_count++;
  result.processed_count++;
  progress.finalized_count++;
  add_error(result.errors, std::format("asset_id {} skipped: {}", asset_id, reason));
}

auto mark_candidate_failed(InfinityNikkiExtractPhotoParamsResult& result,
                           ExtractProgressState& progress, std::int64_t asset_id,
                           const std::string& reason) -> void {
  result.failed_count++;
  result.processed_count++;
  progress.finalized_count++;
  add_error(result.errors, std::format("asset_id {} failed: {}", asset_id, reason));
}

auto mark_candidate_saved(InfinityNikkiExtractPhotoParamsResult& result,
                          ExtractProgressState& progress, std::int32_t clothes_rows_written)
    -> void {
  result.saved_count++;
  result.processed_count++;
  result.clothes_rows_written += clothes_rows_written;
  progress.finalized_count++;
}

auto flush_extract_batch(
    Core::State::AppState& app_state, const std::vector<Scan::PreparedPhotoExtractEntry>& entries,
    InfinityNikkiExtractPhotoParamsResult& result, ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<void> {
  if (entries.empty()) {
    co_return;
  }

  auto fail_all = [&](const std::string& reason) {
    Logger().warn("flush_extract_batch: batch failed (uid={}, count={}): {}", entries.front().uid,
                  entries.size(), reason);
    for (const auto& entry : entries) {
      mark_candidate_failed(result, progress, entry.asset_id, reason);
    }
    report_processing_progress(progress_callback, progress, result, true);
  };

  Logger().debug("flush_extract_batch: sending batch (uid={}, count={})", entries.front().uid,
                 entries.size());

  auto records_result = co_await Infra::extract_batch_photo_params(app_state, entries);
  if (!records_result) {
    Logger().error("flush_extract_batch: extract batch failed: {}", records_result.error());
    fail_all(records_result.error());
    co_return;
  }

  const auto& uid = entries.front().uid;
  const auto& records = records_result.value();
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    if (!records[i].has_value()) {
      mark_candidate_failed(result, progress, entry.asset_id,
                            "API returned null (photo params unrecognized)");
      continue;
    }

    auto save_result =
        Infra::upsert_photo_params_record(app_state, entry.asset_id, uid, *records[i]);
    if (!save_result) {
      Logger().error("flush_extract_batch: DB upsert failed for asset_id={}: {}", entry.asset_id,
                     save_result.error());
      mark_candidate_failed(result, progress, entry.asset_id, save_result.error());
      continue;
    }

    mark_candidate_saved(result, progress, save_result.value());
  }

  report_processing_progress(progress_callback, progress, result, true);
  co_return;
}

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>> {
  InfinityNikkiExtractPhotoParamsResult result;

  if (!app_state.database) {
    co_return std::unexpected("Database is not initialized");
  }

  auto only_missing = request.only_missing.value_or(true);
  auto uid_override = request.uid_override;
  if (uid_override.has_value() && uid_override->empty()) {
    co_return std::unexpected("UID override is empty");
  }

  report_extract_progress(progress_callback, "preparing", 0, 0, kPreparingPercent,
                          "Loading candidate assets");

  auto candidates_result = Infra::load_candidate_assets(app_state, request);
  if (!candidates_result) {
    co_return std::unexpected(candidates_result.error());
  }

  auto candidates = std::move(candidates_result.value());
  result.candidate_count = static_cast<std::int32_t>(candidates.size());

  Logger().info("extract_photo_params: found {} candidate assets (only_missing={})",
                result.candidate_count, only_missing);

  if (candidates.empty()) {
    report_extract_progress(progress_callback, "completed", 0, 0, 100.0, "No candidate assets");
    co_return result;
  }

  ExtractProgressState progress{
      .total_candidates = result.candidate_count,
  };
  report_processing_progress(progress_callback, progress, result, true,
                             std::format("Loaded {} candidate photos", result.candidate_count));

  std::unordered_map<std::string, std::vector<Scan::PreparedPhotoExtractEntry>> active_batches;
  active_batches.reserve(std::min<std::size_t>(candidates.size(), 256));

  for (const auto& candidate : candidates) {
    progress.scanned_count++;

    auto prepared_result = Scan::prepare_photo_extract_entry(candidate, uid_override);
    if (!prepared_result) {
      mark_candidate_skipped(result, progress, candidate.id, prepared_result.error());
      report_processing_progress(progress_callback, progress, result);
      continue;
    }

    auto prepared_entry = std::move(prepared_result.value());
    auto& batch = active_batches[prepared_entry.uid];
    batch.push_back(std::move(prepared_entry));

    report_processing_progress(progress_callback, progress, result);

    if (batch.size() >= kExtractBatchSize) {
      co_await flush_extract_batch(app_state, batch, result, progress, progress_callback);
      batch.clear();
    }
  }

  for (auto& [uid, batch] : active_batches) {
    (void)uid;
    co_await flush_extract_batch(app_state, batch, result, progress, progress_callback);
  }

  Logger().info(
      "extract_photo_params: completed. candidates={}, processed={}, saved={}, skipped={}, "
      "failed={}, clothes_rows={}",
      result.candidate_count, result.processed_count, result.saved_count, result.skipped_count,
      result.failed_count, result.clothes_rows_written);

  report_extract_progress(progress_callback, "completed", result.processed_count,
                          result.candidate_count, 100.0,
                          std::format("Done: saved={}, skipped={}, failed={}", result.saved_count,
                                      result.skipped_count, result.failed_count));

  co_return result;
}

}  // namespace Extensions::InfinityNikki::PhotoExtract
