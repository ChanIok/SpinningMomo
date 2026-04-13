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
constexpr std::size_t kExtractBatchSize = 32;
// 同时放行的远端批次数上限。
// 这里故意保守一点：既提升吞吐，又避免一下子把解析服务或本地网络打满。
constexpr std::size_t kMaxInflightExtractBatches = 4;
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
  std::expected<std::vector<std::optional<Infra::ParsedPhotoParamsRecord>>, std::string> result =
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

auto wait_for_completion(std::atomic<std::size_t>& completed, std::size_t total,
                         const std::function<void(std::size_t)>& on_progress = {})
    -> asio::awaitable<void> {
  // 一个很朴素的“等大家干完”的辅助函数。
  // WorkerPool 和 co_spawn 都没有直接给这里现成的 join/when_all，
  // 所以我们用原子计数 + 小定时器轮询的方式等待全部任务结束。
  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);

  while (true) {
    auto current = completed.load(std::memory_order_relaxed);
    if (on_progress) {
      on_progress(current);
    }
    if (current >= total) {
      break;
    }

    timer.expires_after(std::chrono::milliseconds(kPollIntervalMillis));
    std::error_code wait_error;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, wait_error));
  }
}

auto prepare_entries(
    Core::State::AppState& app_state, const std::vector<Scan::CandidateAssetRow>& candidates,
    const std::optional<std::string>& uid_override, InfinityNikkiExtractPhotoParamsResult& result,
    ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<std::vector<PrepareTaskOutcome>, std::string>> {
  // 第一阶段：把“图库里的候选照片”变成“可以直接发给远端解析服务的数据”。
  // 这里做的事主要是：解析 UID + 打开图片 + 抠出中间藏着的 Base64 字符串。
  std::vector<PrepareTaskOutcome> outcomes(candidates.size());
  if (candidates.empty()) {
    co_return outcomes;
  }

  if (!app_state.worker_pool || !Core::WorkerPool::is_running(*app_state.worker_pool)) {
    co_return std::unexpected("Worker pool is not available for photo extract preparation");
  }

  auto completed = std::make_shared<std::atomic<std::size_t>>(0);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    // 正常路径：把每张照片的本地提取工作扔给 WorkerPool 并发执行。
    // 这一步是磁盘 IO + 少量字符串处理，适合工作线程池。
    auto submitted = Core::WorkerPool::submit_task(
        *app_state.worker_pool,
        [&outcomes, completed, candidate = candidates[index], uid_override, index]() mutable {
          auto prepared_result = Scan::prepare_photo_extract_entry(candidate, uid_override);
          if (prepared_result) {
            outcomes[index].entry = std::move(prepared_result.value());
          } else {
            outcomes[index].error = prepared_result.error();
          }

          completed->fetch_add(1, std::memory_order_relaxed);
        });

    if (!submitted) {
      outcomes[index].error = "Failed to submit prepare task to worker pool";
      completed->fetch_add(1, std::memory_order_relaxed);
    }
  }

  co_await wait_for_completion(*completed, candidates.size(),
                               [&progress, &result, &progress_callback](std::size_t current) {
                                 progress.scanned_count = static_cast<std::int64_t>(current);
                                 report_processing_progress(progress_callback, progress, result);
                               });

  co_return outcomes;
}

auto build_extract_batches(
    const std::vector<Scan::CandidateAssetRow>& candidates,
    std::vector<PrepareTaskOutcome> outcomes, InfinityNikkiExtractPhotoParamsResult& result,
    ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> std::vector<std::vector<Scan::PreparedPhotoExtractEntry>> {
  // 第二阶段：把已经准备好的条目按 UID 分组，再按固定大小切成批次。
  // 因为远端接口要求同一个 batch 内只能有一个 UID。
  std::unordered_map<std::string, std::vector<Scan::PreparedPhotoExtractEntry>> grouped_entries;
  grouped_entries.reserve(std::min<std::size_t>(outcomes.size(), 256));

  for (std::size_t index = 0; index < outcomes.size(); ++index) {
    auto& outcome = outcomes[index];
    if (!outcome.entry.has_value()) {
      mark_candidate_skipped(result, progress, candidates[index].id,
                             outcome.error.value_or("unknown prepare error"));
      continue;
    }

    auto& bucket = grouped_entries[outcome.entry->uid];
    bucket.push_back(std::move(*outcome.entry));
  }

  report_processing_progress(progress_callback, progress, result, true);

  std::vector<std::vector<Scan::PreparedPhotoExtractEntry>> batches;
  for (auto& [uid, entries] : grouped_entries) {
    (void)uid;

    for (std::size_t offset = 0; offset < entries.size(); offset += kExtractBatchSize) {
      auto batch_end = std::min(entries.size(), offset + kExtractBatchSize);

      std::vector<Scan::PreparedPhotoExtractEntry> batch;
      batch.reserve(batch_end - offset);
      for (std::size_t index = offset; index < batch_end; ++index) {
        batch.push_back(std::move(entries[index]));
      }
      batches.push_back(std::move(batch));
    }
  }

  return batches;
}

auto extract_batch_wave(Core::State::AppState& app_state,
                        const std::vector<std::vector<Scan::PreparedPhotoExtractEntry>>& batches,
                        std::size_t wave_start, std::size_t wave_count)
    -> asio::awaitable<std::vector<BatchExtractOutcome>> {
  // 第三阶段：一次只放行少量 batch 并发请求远端。
  // “wave” 可以理解成一小波同时起飞的请求。
  std::vector<BatchExtractOutcome> outcomes(wave_count);
  if (wave_count == 0) {
    co_return outcomes;
  }

  auto executor = co_await asio::this_coro::executor;
  auto completed = std::make_shared<std::atomic<std::size_t>>(0);

  for (std::size_t index = 0; index < wave_count; ++index) {
    auto batch_index = wave_start + index;

    asio::co_spawn(
        executor,
        [&app_state, &batch = batches[batch_index], &outcome = outcomes[index],
         completed]() -> asio::awaitable<void> {
          Logger().debug("extract_batch_wave: sending batch (uid={}, count={})", batch.front().uid,
                         batch.size());
          outcome.result = co_await Infra::extract_batch_photo_params(app_state, batch);
          completed->fetch_add(1, std::memory_order_relaxed);
          co_return;
        },
        asio::detached);
  }

  co_await wait_for_completion(*completed, wave_count);
  co_return outcomes;
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
    Logger().warn("apply_batch_result: batch failed (uid={}, count={}): {}", entries.front().uid,
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

  std::vector<Infra::ParsedPhotoParamsBatchItem> items_to_save;
  items_to_save.reserve(entries.size());

  auto& records = batch_outcome.result.value();
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto& entry = entries[index];
    if (!records[index].has_value()) {
      mark_candidate_failed(result, progress, entry.asset_id,
                            "API returned null (photo params unrecognized)");
      continue;
    }

    items_to_save.push_back(Infra::ParsedPhotoParamsBatchItem{
        .asset_id = entry.asset_id,
        .record = std::move(*records[index]),
    });
  }

  if (items_to_save.empty()) {
    report_processing_progress(progress_callback, progress, result, true);
    return;
  }

  auto save_result =
      Infra::upsert_photo_params_batch(app_state, entries.front().uid, items_to_save);
  if (!save_result) {
    Logger().error("apply_batch_result: DB batch upsert failed (uid={}, count={}): {}",
                   entries.front().uid, items_to_save.size(), save_result.error());
    for (const auto& item : items_to_save) {
      mark_candidate_failed(result, progress, item.asset_id, save_result.error());
    }
    report_processing_progress(progress_callback, progress, result, true);
    return;
  }

  mark_candidates_saved(result, progress, items_to_save.size(), save_result.value());
  report_processing_progress(progress_callback, progress, result, true);
}

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> asio::awaitable<std::expected<InfinityNikkiExtractPhotoParamsResult, std::string>> {
  // 整个流程现在是四步：
  // 1) 查候选照片
  // 2) 并发准备本地提取数据
  // 3) 分批并发请求远端解析
  // 4) 按批落库并汇总结果
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

  report_processing_progress(progress_callback, progress, result, true,
                             std::format("Preparing {} candidate photos", result.candidate_count));

  auto prepare_outcomes_result = co_await prepare_entries(app_state, candidates, uid_override,
                                                          result, progress, progress_callback);
  if (!prepare_outcomes_result) {
    co_return std::unexpected(prepare_outcomes_result.error());
  }

  auto batches = build_extract_batches(candidates, std::move(prepare_outcomes_result.value()),
                                       result, progress, progress_callback);

  for (std::size_t wave_start = 0; wave_start < batches.size();
       wave_start += kMaxInflightExtractBatches) {
    auto wave_count = std::min(kMaxInflightExtractBatches, batches.size() - wave_start);
    auto wave_outcomes = co_await extract_batch_wave(app_state, batches, wave_start, wave_count);

    for (std::size_t index = 0; index < wave_count; ++index) {
      apply_batch_result(app_state, batches[wave_start + index], wave_outcomes[index], result,
                         progress, progress_callback);
    }
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
