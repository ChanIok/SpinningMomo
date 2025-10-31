module;

export module Features.Gallery.Folder.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Folder::Repository {

export auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string>;

export auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string>;

export auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string>;

export auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string>;

export auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

export auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string>;

export auto get_child_folders(Core::State::AppState& app_state,
                              std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string>;

export auto get_folder_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::FolderTreeNode>, std::string>;

}  // namespace Features::Gallery::Folder::Repository
