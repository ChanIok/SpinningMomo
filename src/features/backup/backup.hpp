#pragma once

#include "core/state/app_state.hpp"
#include "features/backup/types.hpp"

namespace Features::Backup {

// 导出数据库、设置、托管背景和迁移版本到单个 ZIP 备份包。
auto export_backup(Core::State::AppState& app_state, const Types::ExportParams& params)
    -> std::expected<Types::ExportResult, std::string>;

// 启动完全替换恢复脚本，当前进程退出后直接解压备份并重启应用。
auto restore_backup(const Types::RestoreParams& params)
    -> std::expected<Types::RestoreResult, std::string>;

}  // namespace Features::Backup
