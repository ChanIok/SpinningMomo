#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::download {

struct DownloadFile {
  std::filesystem::path file_path;
  std::string file_name;
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

// 根据受限归档名解析一个已生成的临时 ZIP。
auto resolve_archive_file(core::AppState& app_state, std::string_view archive_name)
    -> std::expected<DownloadFile, std::string>;

// 完整归档响应结束后删除一次性归档。
auto remove_archive_file(const std::filesystem::path& archive_path) -> void;

}  // namespace features::gallery::download
