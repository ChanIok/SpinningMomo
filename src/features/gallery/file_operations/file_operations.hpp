#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::file_operations {

// 按用户意图删除资产索引，并可选删除对应的物理文件。
auto delete_asset(core::AppState& app_state, const DeleteParams& params)
    -> std::expected<OperationResult, std::string>;

// 使用系统默认应用打开指定资产文件。
auto open_asset_with_default_app(core::AppState& app_state, std::int64_t id)
    -> std::expected<OperationResult, std::string>;

// 在资源管理器中定位指定资产文件。
auto reveal_asset_in_explorer(core::AppState& app_state, std::int64_t id)
    -> std::expected<OperationResult, std::string>;

// 按明确策略处理选中资产；recycle_where_possible 对 UNC 永久删除，其余移入回收站。
auto delete_assets(core::AppState& app_state, const DeleteAssetsParams& params)
    -> std::expected<DeleteAssetsResult, std::string>;

// 将选中资产移动到目标图库文件夹，并同步更新路径索引。
auto move_assets_to_folder(core::AppState& app_state, const MoveAssetsToFolderParams& params)
    -> std::expected<OperationResult, std::string>;

}  // namespace features::gallery::file_operations
