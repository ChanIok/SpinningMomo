module;

export module Features.Gallery.Scanner.Process;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Color.Types;

namespace Features::Gallery::Scanner::Process {

export struct ProcessedAssetEntry {
  Types::Asset asset;
  std::vector<Features::Gallery::Color::Types::ExtractedColor> colors;
};

export struct FileProcessingBatchResult {
  std::vector<ProcessedAssetEntry> new_assets;
  std::vector<ProcessedAssetEntry> updated_assets;
  std::vector<std::string> errors;
};

export struct ProcessingPhaseResult {
  FileProcessingBatchResult batch_result;
};

// 处理阶段：复用目录库存映射 → 并行抽元数据/缩略图/主色 → 批量写库与颜色。
export auto run_processing_phase(
    Core::State::AppState& app_state,
    const std::vector<Types::FileAnalysisResult>& files_to_process,
    const std::unordered_map<std::string, std::int64_t>& folder_mapping,
    const Types::ScanOptions& options,
    const std::function<void(const Types::ScanProgress&)>& progress_callback,
    std::stop_token stop_token) -> std::expected<ProcessingPhaseResult, std::string>;

}  // namespace Features::Gallery::Scanner::Process
