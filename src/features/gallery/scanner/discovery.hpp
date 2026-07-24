#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Scanner::Discovery {

struct DiscoveryResult {
  std::vector<Types::FileSystemInfo> file_infos;
  std::vector<std::filesystem::path> folder_paths;
};

// 发现阶段：一次枚举产出未忽略的目录库存和候选媒体信息。
auto run_discovery_phase(Core::State::AppState& app_state, const std::filesystem::path& directory,
                         std::int64_t folder_id, const Types::ScanOptions& options,
                         const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<DiscoveryResult, std::string>;

}  // namespace Features::Gallery::Scanner::Discovery
