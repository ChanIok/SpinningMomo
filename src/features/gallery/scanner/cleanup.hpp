#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner::cleanup {

struct CleanupPhaseResult {
  int missing_items = 0;
  std::vector<std::string> removed_paths;
};

// 对账阶段：把磁盘上已消失的资产标记为 missing，并清理空目录索引。
auto run_cleanup_phase(core::AppState& app_state, const std::filesystem::path& normalized_scan_root,
                       const std::vector<FileSystemInfo>& file_infos,
                       const std::vector<std::filesystem::path>& folder_paths,
                       const std::unordered_map<std::string, Metadata>& asset_cache,
                       const std::function<void(const ScanProgress&)>& progress_callback)
    -> CleanupPhaseResult;

}  // namespace features::gallery::scanner::cleanup
