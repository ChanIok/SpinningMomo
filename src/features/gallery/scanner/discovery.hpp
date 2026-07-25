#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner::discovery {

struct DiscoveryResult {
  std::vector<FileSystemInfo> file_infos;
  std::vector<std::filesystem::path> folder_paths;
};

// 发现阶段：一次枚举产出未忽略的目录库存和候选媒体信息。
auto run_discovery_phase(core::AppState& app_state, const std::filesystem::path& directory,
                         std::int64_t folder_id, const ScanOptions& options,
                         const std::function<void(const ScanProgress&)>& progress_callback)
    -> std::expected<DiscoveryResult, std::string>;

}  // namespace features::gallery::scanner::discovery
