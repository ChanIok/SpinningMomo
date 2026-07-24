#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Folder::Repository {

auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string>;

auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string>;

auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string>;

auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string>;

auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string>;

auto get_child_folders(Core::State::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string>;

auto get_folder_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::FolderTreeNode>, std::string>;

}  // namespace Features::Gallery::Folder::Repository
