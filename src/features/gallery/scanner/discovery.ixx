module;

export module Features.Gallery.Scanner.Discovery;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner::Discovery {

export struct DiscoveryResult {
  std::vector<Types::FileSystemInfo> file_infos;
  std::vector<std::filesystem::path> folder_paths;
};

// 发现阶段：一次枚举产出未忽略的目录库存和候选媒体信息。
export auto run_discovery_phase(
    Core::State::AppState& app_state, const std::filesystem::path& directory,
    std::int64_t folder_id, const Types::ScanOptions& options,
    const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<DiscoveryResult, std::string>;

}  // namespace Features::Gallery::Scanner::Discovery
