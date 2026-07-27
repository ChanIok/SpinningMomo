#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::folder::repository {

auto create_folder(core::AppState& app_state, const Folder& folder)
    -> std::expected<std::int64_t, std::string>;

auto get_folder_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Folder>, std::string>;

auto get_folder_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Folder>, std::string>;

auto update_folder(core::AppState& app_state, const Folder& folder)
    -> std::expected<void, std::string>;

// 仅在文件夹尚无显示名称时写入名称，并返回本次是否实际更新。
auto update_folder_display_name_if_empty(core::AppState& app_state, std::int64_t folder_id,
                                         const std::string& display_name)
    -> std::expected<bool, std::string>;

auto delete_folder(core::AppState& app_state, std::int64_t id) -> std::expected<void, std::string>;

// 一次读取指定扫描根及其全部子目录，供扫描阶段建立局部目录库存。
auto list_folders_under_root(core::AppState& app_state, const std::string& root_path)
    -> std::expected<std::vector<Folder>, std::string>;

// 在一个事务中按调用方给出的顺序删除目录。
auto batch_delete_folders_by_ids(core::AppState& app_state,
                                 const std::vector<std::int64_t>& folder_ids)
    -> std::expected<void, std::string>;

auto list_all_folders(core::AppState& app_state) -> std::expected<std::vector<Folder>, std::string>;

auto get_child_folders(core::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Folder>, std::string>;

auto get_folder_tree(core::AppState& app_state)
    -> std::expected<std::vector<FolderTreeNode>, std::string>;

}  // namespace features::gallery::folder::repository
