#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/color/types.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Scanner::Process {

struct ProcessedAssetEntry {
  Types::Asset asset;
  std::vector<Features::Gallery::Color::Types::ExtractedColor> colors;
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
auto run_processing_phase(Core::State::AppState& app_state,
                          const std::vector<Types::FileAnalysisResult>& files_to_process,
                          const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                          const Types::ScanOptions& options,
                          const std::function<void(const Types::ScanProgress&)>& progress_callback,
                          std::stop_token stop_token)
    -> std::expected<ProcessingPhaseResult, std::string>;

}  // namespace Features::Gallery::Scanner::Process
