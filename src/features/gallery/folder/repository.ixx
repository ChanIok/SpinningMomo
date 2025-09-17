module;

export module Features.Gallery.Folder.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Folder::Repository {

// ============= 基础CRUD操作 =============

// 创建文件夹记录
auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string>;

// 根据路径获取文件夹
auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string>;

// 根据ID获取文件夹
auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string>;

// 更新文件夹信息
auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string>;

// 删除文件夹（软删除，实际项目可能不需要这个功能）
auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// ============= 层次结构操作 =============

// 获取所有文件夹（用于构建树形结构）
auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string>;

// 获取指定父文件夹的子文件夹
auto get_child_folders(Core::State::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string>;

// 更新文件夹资产计数
auto update_folder_asset_count(Core::State::AppState& app_state, std::int64_t folder_id, int count)
    -> std::expected<void, std::string>;

// 重新计算文件夹资产计数（基于实际assets表数据）
auto recalculate_folder_asset_count(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string>;

// ============= 辅助函数 =============

// 获取或创建文件夹（如果不存在则创建）
auto get_or_create_folder_for_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::int64_t, std::string>;

}  // namespace Features::Gallery::Folder::Repository
