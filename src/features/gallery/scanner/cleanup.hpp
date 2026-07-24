#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Scanner::Cleanup {

struct CleanupPhaseResult {
  int missing_items = 0;
  std::vector<std::string> removed_paths;
};

// 对账阶段：把磁盘上已消失的资产标记为 missing，并清理空目录索引。
auto run_cleanup_phase(Core::State::AppState& app_state,
                       const std::filesystem::path& normalized_scan_root,
                       const std::vector<Types::FileSystemInfo>& file_infos,
                       const std::vector<std::filesystem::path>& folder_paths,
                       const std::unordered_map<std::string, Types::Metadata>& asset_cache,
                       const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> CleanupPhaseResult;

}  // namespace Features::Gallery::Scanner::Cleanup
