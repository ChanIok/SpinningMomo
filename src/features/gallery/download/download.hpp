#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::download {

struct DownloadFile {
  std::filesystem::path file_path;
  std::string file_name;
};

struct ArchiveLease {
  DownloadFile file;
  std::shared_ptr<void> stream_guard;
};

struct PrepareDownloadResult {
  std::optional<std::int64_t> asset_id;
  std::string archive_token;
  std::string file_name;
  std::int64_t failed_count = 0;
};

// 校验选中的资产并准备直链下载或一次性 ZIP 归档。
auto prepare(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<PrepareDownloadResult, std::string>;

// 清理上一次进程遗留的归档和准备目录。
auto cleanup_stale_files() -> void;

// 根据资产 ID 解析一个仍然可用的原始文件。
auto resolve_asset_file(core::AppState& app_state, std::int64_t asset_id)
    -> std::expected<DownloadFile, std::string>;

// 解析已生成的临时 ZIP 并获取其租约卫士（活跃流保护与空闲清理）。
auto acquire_archive_file(core::AppState& app_state, std::string_view archive_name)
    -> std::expected<ArchiveLease, std::string>;

}  // namespace features::gallery::download
