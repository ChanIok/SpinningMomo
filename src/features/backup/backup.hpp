#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/backup/types.hpp"

namespace features::backup {

// 导出数据库、设置、托管背景和迁移版本到单个 ZIP 备份包。
auto export_backup(core::AppState& app_state, const ExportParams& params)
    -> std::expected<ExportResult, std::string>;

// 启动完全替换恢复脚本，当前进程退出后直接解压备份并重启应用。
auto restore_backup(const RestoreParams& params) -> std::expected<RestoreResult, std::string>;

}  // namespace features::backup
