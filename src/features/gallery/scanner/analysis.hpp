#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner::analysis {

// 指纹分析阶段：size/mtime 粗判 → 并行内容指纹 → 产出 NEW/MODIFIED 待处理列表
auto run_hash_analysis_phase(core::AppState& app_state,
                             const std::vector<FileSystemInfo>& file_infos,
                             const std::unordered_map<std::string, Metadata>& asset_cache,
                             const ScanOptions& options,
                             const std::function<void(const ScanProgress&)>& progress_callback,
                             std::stop_token stop_token)
    -> std::expected<std::vector<FileAnalysisResult>, std::string>;

}  // namespace features::gallery::scanner::analysis
