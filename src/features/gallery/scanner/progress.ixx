module;

export module Features.Gallery.Scanner.Progress;

import std;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner::Progress {

// 全量扫描进度百分比锚点：各阶段在自己的区间内插值，避免条来回跳。
export constexpr double kPreparingPercent = 2.0;
export constexpr double kDiscoveringStartPercent = 10.0;
export constexpr double kDiscoveringEndPercent = 25.0;
export constexpr double kHashingStartPercent = 35.0;
export constexpr double kHashingEndPercent = 60.0;
export constexpr std::int64_t kThumbnailProgressWeight = 3;
export constexpr double kProcessingStartPercent = 60.0;
export constexpr double kProcessingEndPercent = 92.0;
export constexpr double kCleanupPercent = 96.0;

// 向 UI 汇报扫描进度；回调抛异常时只记日志，不中断扫描
export auto report_scan_progress(
    const std::function<void(const Types::ScanProgress&)>& progress_callback, std::string stage,
    std::int64_t current, std::int64_t total, std::optional<double> percent = std::nullopt,
    std::optional<std::string> message = std::nullopt) -> void;

// 处理阶段进度：文件数 + 缩略图加权单位，带百分比映射与 200ms 节流
export struct ProcessingProgressTracker {
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
                            double end_percent);

  auto report(bool force = false, std::optional<std::string> message = std::nullopt) -> void;
  auto mark_file_processed() -> void;
  auto mark_thumbnail_processed() -> void;
};

// 指纹阶段进度：候选文件计数 + 百分比映射与 200ms 节流
export struct HashProgressTracker {
  static constexpr std::int64_t kMinReportIntervalMillis = 200;

  const std::function<void(const Types::ScanProgress&)>& progress_callback;
  const std::int64_t total_candidates;
  const double percent_start;
  const double percent_end;

  std::atomic<std::int64_t> completed_candidates = 0;

  std::mutex report_mutex;
  int last_reported_percent = -1;
  std::int64_t last_report_millis = 0;

  HashProgressTracker(const std::function<void(const Types::ScanProgress&)>& callback,
                      std::int64_t candidates, double start_percent, double end_percent);

  auto report(bool force = false) -> void;
  auto mark_item_hashed() -> void;
};

}  // namespace Features::Gallery::Scanner::Progress
