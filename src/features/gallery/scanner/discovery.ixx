module;

export module Features.Gallery.Scanner.Discovery;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner::Discovery {

// 发现阶段：按扩展名与 ignore 规则枚举磁盘候选文件，并读取 size/mtime/ctime
export auto run_discovery_phase(
    Core::State::AppState& app_state, const std::filesystem::path& directory,
    std::int64_t folder_id, const Types::ScanOptions& options,
    const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string>;

}  // namespace Features::Gallery::Scanner::Discovery
