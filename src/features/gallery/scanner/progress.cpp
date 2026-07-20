module;

module Features.Gallery.Scanner.Progress;

import std;
import Features.Gallery.Types;
import Utils.Logger;

namespace Features::Gallery::Scanner::Progress {

auto steady_clock_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// 向 UI 汇报扫描进度；回调抛异常时只记日志，不中断扫描
auto report_scan_progress(const std::function<void(const Types::ScanProgress&)>& progress_callback,
                          std::string stage, std::int64_t current, std::int64_t total,
                          std::optional<double> percent, std::optional<std::string> message)
    -> void {
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

ProcessingProgressTracker::ProcessingProgressTracker(
    const std::function<void(const Types::ScanProgress&)>& callback, std::int64_t files,
    std::int64_t thumbnails, std::int64_t units, std::int64_t thumbnail_weight_value,
    double start_percent, double end_percent)
    : progress_callback(callback),
      total_files(files),
      total_thumbnails(thumbnails),
      total_units(units),
      thumbnail_weight(thumbnail_weight_value),
      percent_start(start_percent),
      percent_end(end_percent) {}

// 按加权完成单位映射到 [percent_start, percent_end]，并节流汇报频率
auto ProcessingProgressTracker::report(bool force, std::optional<std::string> message) -> void {
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

    // 非强制时：整数百分比未前进或间隔过短则跳过
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

    // force 结束时不允许百分比回退
    if (force && last_reported_percent >= 0 &&
        percent < static_cast<double>(last_reported_percent)) {
      percent = static_cast<double>(last_reported_percent);
    }

    last_report_millis = now;
  }

  if (!message.has_value()) {
    if (total_thumbnails > 0) {
      message = std::format("Processed {} / {} files, thumbnails {} / {}", files_done, total_files,
                            thumbnails_done, total_thumbnails);
    } else {
      message = std::format("Processed {} / {} files", files_done, total_files);
    }
  }

  report_scan_progress(progress_callback, "processing", units_done, total_units, percent,
                       std::move(message));
}

auto ProcessingProgressTracker::mark_file_processed() -> void {
  completed_files.fetch_add(1, std::memory_order_relaxed);
  completed_units.fetch_add(1, std::memory_order_relaxed);
  report();
}

auto ProcessingProgressTracker::mark_thumbnail_processed() -> void {
  if (total_thumbnails <= 0) {
    return;
  }

  completed_thumbnails.fetch_add(1, std::memory_order_relaxed);
  completed_units.fetch_add(thumbnail_weight, std::memory_order_relaxed);
  report();
}

HashProgressTracker::HashProgressTracker(
    const std::function<void(const Types::ScanProgress&)>& callback, std::int64_t candidates,
    double start_percent, double end_percent)
    : progress_callback(callback),
      total_candidates(candidates),
      percent_start(start_percent),
      percent_end(end_percent) {}

// 按已算指纹数映射到哈希阶段百分比区间
auto HashProgressTracker::report(bool force) -> void {
  if (!progress_callback || total_candidates <= 0) {
    return;
  }

  auto done = std::min(completed_candidates.load(std::memory_order_relaxed), total_candidates);
  if (force) {
    done = total_candidates;
  }

  const auto ratio = static_cast<double>(done) / static_cast<double>(total_candidates);
  auto percent = percent_start + (percent_end - percent_start) * ratio;
  percent = std::clamp(percent, percent_start, percent_end);

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

  auto message = std::format("Calculating file fingerprints ({} / {})", done, total_candidates);
  report_scan_progress(progress_callback, "hashing", done, total_candidates, percent,
                       std::move(message));
}

auto HashProgressTracker::mark_item_hashed() -> void {
  completed_candidates.fetch_add(1, std::memory_order_relaxed);
  report();
}

}  // namespace Features::Gallery::Scanner::Progress
