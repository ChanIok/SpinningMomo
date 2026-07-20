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

// 清理阶段：盘库对账，删除 DB 有盘无的资产与失效 folder
export auto run_cleanup_phase(
    Core::State::AppState& app_state, const std::filesystem::path& normalized_scan_root,
    const std::vector<Types::FileSystemInfo>& file_infos,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache,
    const std::function<void(const Types::ScanProgress&)>& progress_callback) -> CleanupPhaseResult;

}  // namespace Features::Gallery::Scanner::Cleanup
