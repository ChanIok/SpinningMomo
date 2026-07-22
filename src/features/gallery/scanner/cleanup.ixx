module;

export module Features.Gallery.Scanner.Cleanup;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner::Cleanup {

export struct CleanupPhaseResult {
  int deleted_items = 0;
  std::vector<std::string> removed_paths;
};

// 清理阶段：以文件和目录库存对账，删除磁盘上已消失的索引。
export auto run_cleanup_phase(
    Core::State::AppState& app_state, const std::filesystem::path& normalized_scan_root,
    const std::vector<Types::FileSystemInfo>& file_infos,
    const std::vector<std::filesystem::path>& folder_paths,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache,
    const std::function<void(const Types::ScanProgress&)>& progress_callback) -> CleanupPhaseResult;

}  // namespace Features::Gallery::Scanner::Cleanup
