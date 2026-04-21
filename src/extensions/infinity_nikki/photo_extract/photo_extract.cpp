module;

#include <asio.hpp>

module Extensions.InfinityNikki.PhotoExtract;

import std;
import Core.State;
import Core.WorkerPool;
import Extensions.InfinityNikki.PhotoExtract.Infra;
import Extensions.InfinityNikki.PhotoExtract.Scan;
import Extensions.InfinityNikki.Types;
import Utils.Logger;

namespace Extensions::InfinityNikki::PhotoExtract {

constexpr std::size_t kMaxErrorMessages = 50;
constexpr std::size_t kExtractBatchSize = 50;
constexpr std::int64_t kMinProgressReportIntervalMillis = 200;
constexpr std::int64_t kPollIntervalMillis = 50;
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

struct PrepareTaskOutcome {
  std::optional<Scan::PreparedPhotoExtractEntry> entry;
  std::optional<std::string> error;
};

struct BatchExtractOutcome {
  std::expected<std::vector<Infra::ExtractBatchPhotoParamsRecord>, std::string> result =
      std::unexpected("Batch result unavailable");
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

auto mark_candidates_saved(InfinityNikkiExtractPhotoParamsResult& result,
                           ExtractProgressState& progress, std::size_t saved_count,
                           std::int32_t clothes_rows_written) -> void {
  result.saved_count += static_cast<std::int32_t>(saved_count);
  result.processed_count += static_cast<std::int32_t>(saved_count);
  result.clothes_rows_written += clothes_rows_written;
  progress.finalized_count += static_cast<std::int64_t>(saved_count);
}

auto wait_for_slot_ready(
    std::atomic<bool>& slot_ready, std::atomic<std::size_t>& completed_prepare,
    ExtractProgressState& progress, InfinityNikkiExtractPhotoParamsResult& result,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<void> {
  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);

  while (!slot_ready.load(std::memory_order_acquire)) {
    progress.scanned_count =
        static_cast<std::int64_t>(completed_prepare.load(std::memory_order_relaxed));
    report_processing_progress(progress_callback, progress, result);

    timer.expires_after(std::chrono::milliseconds(kPollIntervalMillis));
    std::error_code wait_error;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, wait_error));
  }

  progress.scanned_count =
      static_cast<std::int64_t>(completed_prepare.load(std::memory_order_relaxed));
  report_processing_progress(progress_callback, progress, result);
}

auto apply_batch_result(
    Core::State::AppState& app_state, const std::vector<Scan::PreparedPhotoExtractEntry>& entries,
    BatchExtractOutcome& batch_outcome, InfinityNikkiExtractPhotoParamsResult& result,
    ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> void {
  // 第四阶段：消费远端返回结果。
  // 能识别的记录收集起来做一次批量落库；识别失败的单条记为 failed。
  if (entries.empty()) {
    return;
  }

  auto fail_all = [&](const std::string& reason) {
    std::unordered_set<std::string> uid_set;
    for (const auto& entry : entries) {
      uid_set.insert(entry.uid);
    }
    Logger().warn("apply_batch_result: batch failed (unique_uids={}, count={}): {}", uid_set.size(),
                  entries.size(), reason);
    for (const auto& entry : entries) {
      mark_candidate_failed(result, progress, entry.asset_id, reason);
    }
    report_processing_progress(progress_callback, progress, result, true);
  };

  if (!batch_outcome.result) {
    Logger().error("apply_batch_result: extract batch failed: {}", batch_outcome.result.error());
    fail_all(batch_outcome.result.error());
    return;
  }

  std::unordered_map<std::string, std::vector<Infra::ParsedPhotoParamsBatchItem>> items_by_uid;

  auto& records = batch_outcome.result.value();
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto& entry = entries[index];
    if (!records[index].record.has_value()) {
      auto reason = records[index].error_message.value_or("photo params unrecognized");
      mark_candidate_failed(result, progress, entry.asset_id,
                            std::format("API returned null ({})", reason));
      continue;
    }

    items_by_uid[entry.uid].push_back(Infra::ParsedPhotoParamsBatchItem{
        .asset_id = entry.asset_id,
        .record = std::move(*records[index].record),
    });
  }

  std::size_t total_saved = 0;
  std::int32_t total_clothes = 0;
  for (const auto& [uid, items] : items_by_uid) {
    if (items.empty()) {
      continue;
    }
    auto save_result = Infra::upsert_photo_params_batch(app_state, uid, items);
    if (!save_result) {
      Logger().error("apply_batch_result: DB batch upsert failed (uid={}, count={}): {}", uid,
                     items.size(), save_result.error());
      for (const auto& item : items) {
        mark_candidate_failed(result, progress, item.asset_id, save_result.error());
      }
      report_processing_progress(progress_callback, progress, result, true);
      return;
    }
    total_saved += items.size();
    total_clothes += save_result.value();
  }

  if (total_saved == 0) {
    report_processing_progress(progress_callback, progress, result, true);
    return;
  }

  mark_candidates_saved(result, progress, total_saved, total_clothes);
  report_processing_progress(progress_callback, progress, result, true);
}

auto send_extract_batch(
    Core::State::AppState& app_state, std::vector<Scan::PreparedPhotoExtractEntry> batch,
    InfinityNikkiExtractPhotoParamsResult& result, ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<void> {
  if (batch.empty()) {
    co_return;
  }
  std::unordered_set<std::string> uid_set;
  for (const auto& entry : batch) {
    uid_set.insert(entry.uid);
  }
  Logger().debug("extract_photo_params: sending batch (unique_uids={}, count={})", uid_set.size(),
                 batch.size());
  BatchExtractOutcome batch_outcome;
  batch_outcome.result = co_await Infra::extract_batch_photo_params(app_state, batch);
  apply_batch_result(app_state, batch, batch_outcome, result, progress, progress_callback);
}

auto extract_photo_params_from_candidates(
    Core::State::AppState& app_state, std::vector<Scan::CandidateAssetRow> candidates,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    std::string_view mode_tag, const std::optional<std::string>& uid_override)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>> {
  // 执行层只关心“候选集合”，不关心候选如何产生（手动 folder / 静默 incremental）。
  InfinityNikkiExtractPhotoParamsResult result;
  result.candidate_count = static_cast<std::int32_t>(candidates.size());

  Logger().info("extract_photo_params: mode={}, found {} candidate assets", mode_tag,
                result.candidate_count);

  if (candidates.empty()) {
    report_extract_progress(progress_callback, "completed", 0, 0, 100.0, "No candidate assets");
    co_return result;
  }

  ExtractProgressState progress{
      .total_candidates = result.candidate_count,
  };
  report_processing_progress(progress_callback, progress, result, true,
                             std::format("Loaded {} candidate photos", result.candidate_count));
  report_processing_progress(progress_callback, progress, result, true,
                             std::format("Preparing {} candidate photos", result.candidate_count));

  if (!app_state.worker_pool || !Core::WorkerPool::is_running(*app_state.worker_pool)) {
    co_return std::unexpected("Worker pool is not available for photo extract preparation");
  }

  const auto candidate_count = candidates.size();
  auto outcomes = std::make_shared<std::vector<PrepareTaskOutcome>>(candidate_count);
  auto completed_prepare = std::make_shared<std::atomic<std::size_t>>(0);
  auto slot_ready = std::make_unique<std::atomic<bool>[]>(candidate_count);
  for (std::size_t i = 0; i < candidate_count; ++i) {
    slot_ready[i].store(false, std::memory_order_relaxed);
  }

  auto* slot_ready_ptr = slot_ready.get();
  for (std::size_t index = 0; index < candidate_count; ++index) {
    auto submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool, [outcomes, completed_prepare, slot_ready_ptr,
                                 candidate = candidates[index], uid_override, index]() mutable {
          auto prepared_result = Scan::prepare_photo_extract_entry(candidate, uid_override);
          if (prepared_result) {
            (*outcomes)[index].entry = std::move(prepared_result.value());
          } else {
            (*outcomes)[index].error = prepared_result.error();
          }

          slot_ready_ptr[index].store(true, std::memory_order_release);
          completed_prepare->fetch_add(1, std::memory_order_relaxed);
        });

    if (!submitted) {
      (*outcomes)[index].error = "Failed to submit prepare task to worker pool";
      slot_ready_ptr[index].store(true, std::memory_order_release);
      completed_prepare->fetch_add(1, std::memory_order_relaxed);
    }
  }

  std::vector<Scan::PreparedPhotoExtractEntry> pending_batch;
  pending_batch.reserve(kExtractBatchSize);

  for (std::size_t index = 0; index < candidate_count; ++index) {
    co_await wait_for_slot_ready(slot_ready_ptr[index], *completed_prepare, progress, result,
                                 progress_callback);

    auto& outcome = (*outcomes)[index];
    if (!outcome.entry.has_value()) {
      mark_candidate_skipped(result, progress, candidates[index].id,
                             outcome.error.value_or("unknown prepare error"));
      continue;
    }

    auto entry = std::move(*outcome.entry);
    pending_batch.push_back(std::move(entry));

    if (pending_batch.size() >= kExtractBatchSize) {
      co_await send_extract_batch(app_state, std::move(pending_batch), result, progress,
                                  progress_callback);
      pending_batch.clear();
      pending_batch.reserve(kExtractBatchSize);
    }
  }

  progress.scanned_count = static_cast<std::int64_t>(candidate_count);
  report_processing_progress(progress_callback, progress, result, true);
  co_await send_extract_batch(app_state, std::move(pending_batch), result, progress,
                              progress_callback);

  Logger().info(
      "extract_photo_params: mode={}, completed. candidates={}, processed={}, saved={}, "
      "skipped={}, failed={}, clothes_rows={}",
      mode_tag, result.candidate_count, result.processed_count, result.saved_count,
      result.skipped_count, result.failed_count, result.clothes_rows_written);

  report_extract_progress(progress_callback, "completed", result.processed_count,
                          result.candidate_count, 100.0,
                          std::format("Done: saved={}, skipped={}, failed={}", result.saved_count,
                                      result.skipped_count, result.failed_count));
  co_return result;
}

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>> {
  // 整个流程：
  // 1) 查候选照片
  // 2) 并发准备本地提取数据（WorkerPool），按候选顺序等待每个 slot
  // 3) 边准备边分批请求远端（每批最多 kExtractBatchSize，可含多 UID），顺序落库
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
  // 手动任务保留原语义：候选由 folder_id/only_missing 决定。
  Logger().info("extract_photo_params: mode=manual_task, only_missing={}", only_missing);
  co_return co_await extract_photo_params_from_candidates(
      app_state, std::move(candidates), progress_callback, "manual_task", uid_override);
}

auto extract_photo_params_silent_incremental(
    Core::State::AppState& app_state, const InfinityNikkiSilentExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>> {
  if (!app_state.database) {
    co_return std::unexpected("Database is not initialized");
  }

  report_extract_progress(progress_callback, "preparing", 0, 0, kPreparingPercent,
                          "Loading incremental candidate assets");
  Logger().debug("extract_photo_params: mode=silent_incremental, requested_ids={}",
                 request.candidate_asset_ids.size());

  auto candidates_result =
      Infra::load_candidate_assets_by_ids(app_state, request.candidate_asset_ids);
  if (!candidates_result) {
    co_return std::unexpected(candidates_result.error());
  }

  auto candidates = std::move(candidates_result.value());
  // 静默增量明确只解析本次变更集映射出的资产，不混入历史 missing。
  Logger().info("extract_photo_params: mode=silent_incremental, resolved_candidates={}",
                candidates.size());
  co_return co_await extract_photo_params_from_candidates(
      app_state, std::move(candidates), progress_callback, "silent_incremental", std::nullopt);
}

}  // namespace Extensions::InfinityNikki::PhotoExtract
