module;

export module Features.Gallery.Scanner.Analysis;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner::Analysis {

// 指纹分析阶段：size/mtime 粗判 → 并行内容指纹 → 产出 NEW/MODIFIED 待处理列表
export auto run_hash_analysis_phase(
    Core::State::AppState& app_state, const std::vector<Types::FileSystemInfo>& file_infos,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache,
    const Types::ScanOptions& options,
    const std::function<void(const Types::ScanProgress&)>& progress_callback,
    std::stop_token stop_token)
    -> std::expected<std::vector<Types::FileAnalysisResult>, std::string>;

}  // namespace Features::Gallery::Scanner::Analysis
