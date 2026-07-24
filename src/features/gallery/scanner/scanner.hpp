#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner {

// 全量同步一个目录：准备 → 盘点文件/目录 → 同步目录库存 → 处理资产 → 清理 → 组装 ScanChange。
auto scan_asset_directory(core::AppState& app_state, const ScanOptions& options,
                          std::function<void(const ScanProgress&)> progress_callback = nullptr)
    -> std::expected<ScanResult, std::string>;

// 同步物化应用主动创建的单个文件，并返回已经落库的真实扫描变化。
auto upsert_created_file(core::AppState& app_state, std::int64_t folder_id,
                         const std::filesystem::path& path)
    -> std::expected<ScanResult, std::string>;

}  // namespace features::gallery::scanner
