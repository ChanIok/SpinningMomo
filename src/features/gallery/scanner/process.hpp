#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/color/types.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner::process {

struct ProcessedAssetEntry {
  Asset asset;
  std::vector<features::gallery::color::ExtractedColor> colors;
};

struct FileProcessingBatchResult {
  std::vector<ProcessedAssetEntry> new_assets;
  std::vector<ProcessedAssetEntry> updated_assets;
  std::vector<std::string> errors;
};

struct ProcessingPhaseResult {
  FileProcessingBatchResult batch_result;
};

// 处理阶段：复用目录库存映射 → 并行抽元数据/缩略图/主色 → 批量写库与颜色。
auto run_processing_phase(core::AppState& app_state,
                          const std::vector<FileAnalysisResult>& files_to_process,
                          const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                          const ScanOptions& options,
                          const std::function<void(const ScanProgress&)>& progress_callback,
                          std::stop_token stop_token)
    -> std::expected<ProcessingPhaseResult, std::string>;

}  // namespace features::gallery::scanner::process
