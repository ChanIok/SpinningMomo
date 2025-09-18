module;

export module Features.Gallery.Folder.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Folder::Repository {

// 创建文件夹记录
export auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string>;

// 根据路径获取文件夹
export auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string>;

// 根据ID获取文件夹
export auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string>;

// 更新文件夹信息
export auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string>;

// 删除文件夹（软删除，实际项目可能不需要这个功能）
export auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// 获取所有文件夹（用于构建树形结构）
export auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string>;

// 获取指定父文件夹的子文件夹
export auto get_child_folders(Core::State::AppState& app_state,
                              std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string>;

// 获取或创建文件夹（如果不存在则创建）
export auto get_or_create_folder_for_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::int64_t, std::string>;

}  // namespace Features::Gallery::Folder::Repository
