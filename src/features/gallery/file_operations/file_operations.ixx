module;

export module Features.Gallery.FileOperations;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::FileOperations {

// 按用户意图删除资产索引，并可选删除对应的物理文件。
export auto delete_asset(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::OperationResult, std::string>;

// 使用系统默认应用打开指定资产文件。
export auto open_asset_with_default_app(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string>;

// 在资源管理器中定位指定资产文件。
export auto reveal_asset_in_explorer(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string>;

// 按明确策略处理选中资产；recycle_where_possible 对 UNC 永久删除，其余移入回收站。
export auto delete_assets(Core::State::AppState& app_state, const Types::DeleteAssetsParams& params)
    -> std::expected<Types::DeleteAssetsResult, std::string>;

// 将选中资产移动到目标图库文件夹，并同步更新路径索引。
export auto move_assets_to_folder(Core::State::AppState& app_state,
                                  const Types::MoveAssetsToFolderParams& params)
    -> std::expected<Types::OperationResult, std::string>;

}  // namespace Features::Gallery::FileOperations
